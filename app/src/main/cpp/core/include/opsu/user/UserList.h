#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "opsu/user/User.h"

namespace opsu {

/**
 * Persistence seam for UserList: the five ScoreDB calls the Java class makes
 * (getUsers, getCurrentUser, setCurrentUser, updateUser, deleteUser).
 * The SQLite-backed ScoreDB will implement this; tests use an in-memory one.
 */
class UserStore {
public:
    virtual ~UserStore() = default;
    virtual std::vector<User> getUsers() = 0;
    /** nullopt == Java null (no current user saved). */
    virtual std::optional<std::string> getCurrentUser() = 0;
    virtual void setCurrentUser(const std::string& name) = 0;
    virtual void updateUser(const User& user) = 0;
    virtual void deleteUser(const std::string& name) = 0;
};

/** List of users. Port of itdelatrisu.opsu.user.UserList. */
class UserList {
public:
    static constexpr const char* DEFAULT_USER_NAME = "Guest";
    static constexpr int DEFAULT_ICON = 0;
    /** The name of the "user" when the "Auto" mod is active (OpsuConstants::PROJECT_NAME). */
    static const char* autoUserName();
    static constexpr int MAX_USER_NAME_LENGTH = 16;

    /** Creates a new instance (overwriting any previous one). `store` must outlive the list. */
    static void create(UserStore& store);
    /** The single instance (nullptr before create(), like the Java static). */
    static UserList* get();

    /** Builds the user list (loads from the store; creates "Guest" when needed). */
    explicit UserList(UserStore& store);

    UserList(const UserList&) = delete;
    UserList& operator=(const UserList&) = delete;

    int size() const { return (int)users.size(); }
    /** All users, sorted by name (case-insensitive). Copies, like the Java list of references would be re-sorted. */
    std::vector<User> getUsers() const;
    /** Never null after construction. */
    User* getCurrentUser() { return currentUser; }
    const User* getCurrentUser() const { return currentUser; }

    bool userExists(const std::string& name) const;
    /** nullptr if none. Pointers stay valid until that user is deleted. */
    User* getUser(const std::string& name);

    /** @return true if the user changed. */
    bool changeUser(const std::string& name);
    /** @return the new User, or nullptr if the name is invalid (e.g. exists). */
    User* createNewUser(const std::string& name, int icon);
    /** @return true if deleted (false if unknown or the current user). */
    bool deleteUser(const std::string& name);
    bool isValidUserName(const std::string& name) const;

private:
    UserStore& store;
    /** Keyed by lower-cased name (ASCII folding; Java used String.toLowerCase()). */
    std::map<std::string, User> users;
    User* currentUser = nullptr;
};

}  // namespace opsu
