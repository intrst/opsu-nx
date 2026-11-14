// Tests for batch 11 (UserList). Behavioural, no JVM golden (nothing float/locale dependent).
#include <cstdio>
#include <map>
#include "opsu/user/UserList.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

/** In-memory stand-in for ScoreDB's user table. */
struct MemStore : UserStore {
    std::vector<User> rows;
    std::optional<std::string> current;
    int setCalls = 0, updateCalls = 0, deleteCalls = 0;
    std::vector<User> getUsers() override { return rows; }
    std::optional<std::string> getCurrentUser() override { return current; }
    void setCurrentUser(const std::string& n) override { current = n; setCalls++; }
    void updateUser(const User& u) override {
        updateCalls++;
        for (User& r : rows) if (r.getName() == u.getName()) { r = u; return; }
        rows.push_back(u);
    }
    void deleteUser(const std::string& n) override {
        deleteCalls++;
        for (size_t i = 0; i < rows.size(); i++) if (rows[i].getName() == n) { rows.erase(rows.begin() + i); return; }
    }
};

int main() {
    // empty store: Guest is created and made current
    {
        MemStore st;
        UserList l(st);
        CHECK(l.size() == 1);
        CHECK(l.getCurrentUser() && l.getCurrentUser()->getName() == "Guest");
        CHECK(st.updateCalls == 1 && st.rows.size() == 1 && st.current == std::string("Guest"));
        CHECK(l.userExists("GUEST") && l.userExists("guest"));
        CHECK(!l.userExists(""));
    }
    // existing users: saved current user is restored, nothing is created
    {
        MemStore st;
        st.rows = {User("Zed", 2), User("alice", 1), User("Bob", 3)};
        st.current = std::string("BOB");   // lookup is case-insensitive; stored name keeps its case
        UserList l(st);
        CHECK(l.size() == 3 && st.updateCalls == 0);
        CHECK(l.getCurrentUser()->getName() == "Bob");
        CHECK(st.current == std::string("BOB"));   // changeUser writes back the name it was given
        auto sorted = l.getUsers();
        CHECK(sorted.size() == 3 && sorted[0].getName() == "alice" && sorted[1].getName() == "Bob" && sorted[2].getName() == "Zed");
    }
    // saved current user missing / null: falls back to Guest (created if absent, reused if present)
    {
        MemStore st;
        st.rows = {User("Bob", 3)};
        st.current = std::string("nobody");
        UserList l(st);
        CHECK(l.size() == 2 && st.updateCalls == 1);
        CHECK(l.getCurrentUser()->getName() == "Guest");
        MemStore st2;
        st2.rows = {User("Bob", 3)};   // current == null
        UserList l2(st2);
        CHECK(l2.getCurrentUser()->getName() == "Guest" && l2.size() == 2);
        MemStore st3;
        st3.rows = {User("guest", 5), User("Bob", 3)};
        st3.current = std::string("missing");
        UserList l3(st3);
        CHECK(l3.size() == 2 && st3.updateCalls == 0);
        CHECK(l3.getCurrentUser()->getName() == "guest" && l3.getCurrentUser()->getIconId() == 5);
    }
    // name validation
    {
        MemStore st;
        UserList l(st);
        CHECK(l.isValidUserName("Alice"));
        CHECK(!l.isValidUserName(""));
        CHECK(l.isValidUserName("0123456789abcdef"));    // 16 units: allowed
        CHECK(!l.isValidUserName("0123456789abcdefg"));  // 17: too long
        CHECK(!l.isValidUserName(" Alice") && !l.isValidUserName("Alice ") && !l.isValidUserName("Al\tice\n"));
        CHECK(l.isValidUserName("Al ice"));              // inner spaces are fine
        CHECK(!l.isValidUserName("guest") && !l.isValidUserName("GUEST"));   // exists, case-insensitive
        CHECK(!l.isValidUserName("OPSU!") && !l.isValidUserName("opsu!"));   // the Auto user's name
        CHECK(l.isValidUserName("opsu"));
        // length is counted in UTF-16 units: 8 x U+1F600 (surrogate pairs) = 16 units ok, 9 = 18 not
        std::string e = "\xF0\x9F\x98\x80";
        std::string s8, s9;
        for (int i = 0; i < 8; i++) s8 += e;
        s9 = s8 + e;
        CHECK(l.isValidUserName(s8) && !l.isValidUserName(s9));
        CHECK(l.isValidUserName("\xC3\xA9"));            // non-ASCII is not trimmed away
    }
    // create / change / delete
    {
        MemStore st;
        UserList l(st);
        User* a = l.createNewUser("Alice", 4);
        CHECK(a && a->getName() == "Alice" && a->getIconId() == 4 && l.size() == 2 && st.rows.size() == 2);
        CHECK(l.createNewUser("alice", 1) == nullptr && l.size() == 2);   // duplicate
        CHECK(l.createNewUser("", 1) == nullptr);
        CHECK(l.getUser("ALICE") == a && l.getUser("nope") == nullptr);

        int before = st.setCalls;
        CHECK(!l.changeUser("nope") && st.setCalls == before && l.getCurrentUser()->getName() == "Guest");
        CHECK(l.changeUser("alice") && l.getCurrentUser() == a && st.current == std::string("alice"));

        CHECK(!l.deleteUser("Alice"));      // current user
        CHECK(!l.deleteUser("alice"));      // (deviation: Java's case-sensitive check would have allowed this)
        CHECK(!l.deleteUser("nope") && st.deleteCalls == 0);
        CHECK(l.deleteUser("GUEST") && st.deleteCalls == 1 && l.size() == 1 && !l.userExists("Guest"));
        CHECK(l.getCurrentUser()->getName() == "Alice");   // still valid
    }
    // singleton
    {
        MemStore st;
        CHECK(UserList::get() == nullptr || true);
        UserList::create(st);
        UserList* first = UserList::get();
        CHECK(first && first->size() == 1);
        UserList::create(st);   // overwrites
        CHECK(UserList::get() != nullptr && UserList::get()->size() == 1);
    }

    if (failures) { std::printf("batch11: %d FAILED\n", failures); return 1; }
    std::printf("batch11: all tests passed\n");
    return 0;
}
