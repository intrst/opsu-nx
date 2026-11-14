#include "opsu/user/UserList.h"
#include <algorithm>
#include "opsu/OpsuConstants.h"
#include "opsu/util/JavaIO.h"

namespace opsu {

namespace {
std::unique_ptr<UserList> g_list;

std::string lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    return r;
}

/** Java String.trim(): strips chars <= U+0020 from both ends (UTF-8 lead/continuation bytes are all >= 0x80). */
std::string javaTrim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (unsigned char)s[b] <= 0x20) b++;
    while (e > b && (unsigned char)s[e - 1] <= 0x20) e--;
    return s.substr(b, e - b);
}
}  // namespace

const char* UserList::autoUserName() { return OpsuConstants::PROJECT_NAME; }

void UserList::create(UserStore& store) { g_list = std::make_unique<UserList>(store); }
UserList* UserList::get() { return g_list.get(); }

UserList::UserList(UserStore& s) : store(s) {
    std::vector<User> all = store.getUsers();
    for (const User& u : all) users.insert_or_assign(lower(u.getName()), u);

    if (all.empty()) {
        // create the default user
        createNewUser(DEFAULT_USER_NAME, DEFAULT_ICON);
        changeUser(DEFAULT_USER_NAME);
    } else {
        // find the current user
        std::optional<std::string> cur = store.getCurrentUser();
        if (!cur || !changeUser(*cur)) {
            // user not found: use the default user (create if needed)
            if (!userExists(DEFAULT_USER_NAME))
                createNewUser(DEFAULT_USER_NAME, DEFAULT_ICON);
            changeUser(DEFAULT_USER_NAME);
        }
    }
}

std::vector<User> UserList::getUsers() const {
    std::vector<User> l;
    l.reserve(users.size());
    for (const auto& kv : users) l.push_back(kv.second);
    std::stable_sort(l.begin(), l.end(), [](const User& a, const User& b) { return a.compareTo(b) < 0; });
    return l;
}

bool UserList::userExists(const std::string& name) const { return users.count(lower(name)) > 0; }

User* UserList::getUser(const std::string& name) {
    auto it = users.find(lower(name));
    return it == users.end() ? nullptr : &it->second;
}

bool UserList::changeUser(const std::string& name) {
    if (!userExists(name)) return false;
    currentUser = getUser(name);
    store.setCurrentUser(name);
    return true;
}

User* UserList::createNewUser(const std::string& name, int icon) {
    if (!isValidUserName(name)) return nullptr;
    auto it = users.insert_or_assign(lower(name), User(name, icon)).first;
    store.updateUser(it->second);
    return &it->second;
}

bool UserList::deleteUser(const std::string& name) {
    // Deviation: Java compared `name.equals(currentUser.getName())` (case-sensitive), so deleting "guest" while "Guest" was
    // current removed the current user from the map. Here that would leave `currentUser` dangling, so the check is
    // case-insensitive like the map key.
    if (!userExists(name) || (currentUser && lower(name) == lower(currentUser->getName()))) return false;
    store.deleteUser(name);
    users.erase(lower(name));
    return true;
}

bool UserList::isValidUserName(const std::string& name) const {
    return !name.empty() && java::utf16Length(name) <= (size_t)MAX_USER_NAME_LENGTH &&
           name == javaTrim(name) && !userExists(name) &&
           java::compareToIgnoreCase(name, autoUserName()) != 0;
}

}  // namespace opsu
