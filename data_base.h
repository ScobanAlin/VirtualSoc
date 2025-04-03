#pragma once
#include <sqlite3.h>
#include <iostream>
#include <mutex>
#include <stdexcept> 
using namespace std;

mutex db_mutex;

void DB_SETUP(sqlite3 *&db, char *db_error)
{
    sqlite3_config(SQLITE_CONFIG_MEMSTATUS);
    sqlite3_config(SQLITE_CONFIG_PAGECACHE, 4096, 1024, 0);

    int rc = sqlite3_open_v2("DATA.db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK)
    {
        cerr << "Error opening database: " << sqlite3_errmsg(db) << endl;
    }

    const char *CREATE_USERS_TABLE = R"(
        CREATE TABLE IF NOT EXISTS Users (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT, 
            username TEXT UNIQUE NOT NULL,             
            password TEXT NOT NULL,               
            profile_privacy TEXT CHECK(profile_privacy IN ('public', 'private')) DEFAULT 'public',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP 
        );
    )";

    rc = sqlite3_exec(db, CREATE_USERS_TABLE, nullptr, nullptr, &db_error);
    if (rc != SQLITE_OK)
    {
        cerr << "Error creating Users table: " << db_error << endl;
        throw runtime_error("Failed to create Users table");
    }

    const char *CREATE_POSTS_TABLE = R"(
        CREATE TABLE IF NOT EXISTS Posts (
            post_id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL,
            content TEXT NOT NULL,
            privacy TEXT CHECK(privacy IN ('public', 'close', 'private')) DEFAULT 'public',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (username) REFERENCES Users(username)
        );
    )";

    rc = sqlite3_exec(db, CREATE_POSTS_TABLE, nullptr, nullptr, &db_error);
    if (rc != SQLITE_OK)
    {
        cerr << "Error creating Posts table: " << db_error << endl;
        throw runtime_error("Failed to create Posts table");
    }

    const char *CREATE_FRIENDS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS Friends (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        requester TEXT NOT NULL,
        requested TEXT NOT NULL,
        friendship_type TEXT CHECK(friendship_type IN ('close', 'normal')) DEFAULT 'normal',
        status TEXT CHECK(status IN ('pending', 'accepted', 'rejected')) DEFAULT 'pending',
        FOREIGN KEY (requester) REFERENCES Users(username),
        FOREIGN KEY (requested) REFERENCES Users(username)
    );
)";

    rc = sqlite3_exec(db, CREATE_FRIENDS_TABLE, nullptr, nullptr, &db_error);
    if (rc != SQLITE_OK)
    {
        cerr << "Error creating Friends table: " << db_error << endl;
        throw runtime_error("Failed to create Friends table");
    }

    const char *CREATE_GROUPS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS Groups (
        group_id INTEGER PRIMARY KEY AUTOINCREMENT, 
        group_name TEXT NOT NULL, 
        users TEXT NOT NULL
    );
)";

    rc = sqlite3_exec(db, CREATE_GROUPS_TABLE, nullptr, nullptr, &db_error);
    if (rc != SQLITE_OK)
    {
        cerr << "Error creating Groups table: " << db_error << endl;
        throw runtime_error("Failed to create Groups table");
    }

    const char *CREATE_MESSAGES_TABLE = R"(
    CREATE TABLE IF NOT EXISTS Messages (
        message_id INTEGER PRIMARY KEY AUTOINCREMENT,
        group_id INTEGER NOT NULL,
        username TEXT NOT NULL,
        content TEXT NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (group_id) REFERENCES Groups(group_id),
        FOREIGN KEY (username) REFERENCES Users(username)
    );
)";

    rc = sqlite3_exec(db, CREATE_MESSAGES_TABLE, nullptr, nullptr, &db_error);
    if (rc != SQLITE_OK)
    {
        cerr << "Error creating Messages table: " << db_error << endl;
        throw runtime_error("Failed to create Messages table");
    }
}

bool DB_CHECK_USER(sqlite3 *db, char *db_error, const char *username)
{

    const char *CHECK_USERNAME = R"(
        SELECT COUNT(*) FROM Users WHERE username = ?;
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, CHECK_USERNAME, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW)
        {
            cerr << "Error checking username existence: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (count > 0)
        {
            return false;
        }
        return true;
        
    }
}

bool DB_REGISTER_USER(sqlite3 *db, char *db_error, const char *username, const char *password)
{
    if (DB_CHECK_USER(db, db_error, username) == false)
    {
        return false;
    }
    const char *INSERT_USER = R"(
        INSERT INTO Users (username, password)
        VALUES (?, ?);
    )";

    sqlite3_stmt *stmt;
    int rc;


    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, INSERT_USER, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "Error inserting user: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        cout << "Client registered successfully!" << endl;

        sqlite3_finalize(stmt);
    }

    return true;
}

bool DB_LOGIN_USER(sqlite3 *db, char *db_error, const char *username, const char *password)
{
    const char *SELECT_USER = R"(
        SELECT password FROM Users WHERE username = ?;
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);
        rc = sqlite3_prepare_v2(db, SELECT_USER, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {
            const char *stored_password = (const char *)(sqlite3_column_text(stmt, 0));

            if (stored_password != nullptr && strcmp(stored_password, password) == 0)
            {
                sqlite3_finalize(stmt);
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    sqlite3_finalize(stmt);
    return true;
}

bool DB_POST_INSTERT(sqlite3 *db, char *db_error, const char *username, const char *content, char *privacy)
{
    const char *INSERT_POST = R"(
        INSERT INTO Posts (username, content, privacy)
        VALUES (?, ?, ?);
    )";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, INSERT_POST, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        cerr << "Error preparing insert post query: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, content, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, privacy, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        cerr << "Error inserting post: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool DB_GET_PUBLIC_POSTS(sqlite3 *db, char *db_error, string &result)
{
    const char *SELECT_PUBLIC_POSTS = R"(
        SELECT username, content,created_at FROM Posts WHERE privacy = 'public';
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, SELECT_PUBLIC_POSTS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        result.clear();

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const char *username = (const char *)(sqlite3_column_text(stmt, 0));
            const char *content = (const char *)(sqlite3_column_text(stmt, 1));
            const char *time = (const char *)(sqlite3_column_text(stmt, 2));

            if (username != nullptr && content != nullptr)
            {
                result += username;
                result += "~";
                result += content;
                result += "~";
                result += time;
                result += "~";
            }
        }

        if (rc != SQLITE_DONE)
        {
            cerr << "Error stepping through rows: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}

bool DB_CHECK_FRIENDSHIP(sqlite3 *db, char *db_error, const char *user1, const char *user2)
{
    const char *CHECK_FRIENDSHIP_QUERY = R"(
        SELECT 1
        FROM Friends
        WHERE (requester = ? AND requested = ?)
           OR (requester = ? AND requested = ?);
    )";

    sqlite3_stmt *stmt;
    int rc;
    bool friendship_exists = false;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, CHECK_FRIENDSHIP_QUERY, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, user1, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user1, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {
            friendship_exists = true;
        }
        else if (rc != SQLITE_DONE)
        {
            cerr << "Error checking friendship: " << sqlite3_errmsg(db) << endl;
        }

        sqlite3_finalize(stmt);
    }

    return friendship_exists;
}

bool DB_ADD_FRIEND(sqlite3 *db, char *db_error, const char *requester, const char *requested, const char *friendship_type)
{
    const char *INSERT_FRIEND_REQUEST = R"(
        INSERT INTO Friends (requester, requested, friendship_type)
        VALUES (?, ?, ?);
    )";

    sqlite3_stmt *stmt;
    int rc;
    
    if (DB_CHECK_FRIENDSHIP(db, db_error, requester, requested))
    {
        cerr << "Friendship already exists between " << requester << " and " << requested << "." << endl;
        return false;
    }

    if (DB_CHECK_USER(db, db_error, requester) ||
        DB_CHECK_USER(db, db_error, requested) or strcmp(requester,requested) == 0)
    {
        cerr << "One or both users do not exist." << endl;
        return false;
    }





    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, INSERT_FRIEND_REQUEST, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, requester, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, requested, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, friendship_type, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "Error adding friend request: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    cout << "Friend request sent successfully!" << endl;
    return true;
}

bool DB_GET_MY_FRIENDS(sqlite3 *db, char *db_error, const char *username, string &message)
{
    const char *SELECT_FRIENDS = R"(
        SELECT CASE
                   WHEN requester = ? THEN requested
                   WHEN requested = ? THEN requester
               END AS friend,
               friendship_type
        FROM Friends
        WHERE (requester = ? OR requested = ?) AND status = 'accepted'
        ORDER BY friendship_type ASC ,friend ASC;
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, SELECT_FRIENDS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, username, -1, SQLITE_STATIC);

        message.clear(); 

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const char *friend_username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            const char *friendship_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

            if (friend_username != nullptr && friendship_type != nullptr)
            {
                message += " "; 
                message += friend_username;
                message += " ";
                message += friendship_type;
            }
        }

        if (rc != SQLITE_DONE)
        {
            cerr << "Error stepping through rows: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}



bool DB_GET_FRIEND_REQUESTS(sqlite3 *db, char *db_error, const char *username, string &message)
{
    const char *SELECT_FRIEND_REQUESTS = R"(
        SELECT requester, friendship_type
        FROM Friends
        WHERE requested = ? AND status = 'pending';
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, SELECT_FRIEND_REQUESTS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        message.clear(); 

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const char *requester_username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            const char *friendship_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

            if (requester_username != nullptr && friendship_type != nullptr)
            {
                message += " ";
                message += requester_username;
                message += " ";
                message += friendship_type;
            }
        }

        if (rc != SQLITE_DONE)
        {
            cerr << "Error stepping through rows: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}



bool DB_ACCEPT_FRIENDSHIP(sqlite3 *db, char *db_error, const char *user1, const char *user2)
{
    const char *UPDATE_FRIENDSHIP_STATUS = R"(
        UPDATE Friends
        SET status = 'accepted'
        WHERE ((requester = ? AND requested = ?) OR (requester = ? AND requested = ?))
        AND status = 'pending';
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, UPDATE_FRIENDSHIP_STATUS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing UPDATE statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, user1, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user1, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "Error updating friendship status: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    cout << "Friendship accepted between " << user1 << " and " << user2 << "." << endl;
    return true;
}



bool DB_REJECT_FRIENDSHIP(sqlite3 *db, char *db_error, const char *user1, const char *user2)
{
    const char *DELETE_FRIENDSHIP_REQUEST = R"(
        DELETE FROM Friends
        WHERE ((requester = ? AND requested = ?) OR (requester = ? AND requested = ?))
        AND status = 'pending';
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, DELETE_FRIENDSHIP_REQUEST, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing DELETE statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, user1, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user1, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "Error deleting friendship request: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    cout << "Friendship request between " << user1 << " and " << user2 << " has been rejected." << endl;
    return true;
}

bool DB_DELETE_FRIENDSHIP(sqlite3 *db, char *db_error, const char *user1, const char *user2)
{
    const char *DELETE_FRIENDSHIP = R"(
        DELETE FROM Friends
        WHERE (requester = ? AND requested = ?) OR (requester = ? AND requested = ?);
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, DELETE_FRIENDSHIP, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing DELETE statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, user1, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user1, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "Error deleting friendship: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    cout << "Friendship between " << user1 << " and " << user2 << " has been deleted." << endl;
    return true;
}


bool DB_GET_FRIEND_POSTS(sqlite3 *db, char *db_error, const char *username, string &result)
{
    const char *SELECT_PRIVATE_POSTS = R"(
        SELECT p.username, p.content, p.created_at
        FROM Posts p
        JOIN Friends f
        ON (f.requester = ? AND f.requested = p.username OR f.requester = p.username AND f.requested = ?)
        WHERE f.status = 'accepted' AND p.privacy = 'private';
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, SELECT_PRIVATE_POSTS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);

        result.clear();

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const char *post_username = (const char *)(sqlite3_column_text(stmt, 0));
            const char *content = (const char *)(sqlite3_column_text(stmt, 1));
            const char *time = (const char *)(sqlite3_column_text(stmt, 2));

            if (post_username != nullptr && content != nullptr && time != nullptr)
            {
                result += post_username;
                result += "~";
                result += content;
                result += "~";
                result += time;
                result += "~";
            }
        }

        if (rc != SQLITE_DONE)
        {
            cerr << "Error stepping through rows: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}

bool DB_GET_CLOSE_POSTS(sqlite3 *db, char *db_error, const char *username, string &result)
{
    const char *SELECT_PRIVATE_POSTS = R"(
        SELECT p.username, p.content, p.created_at
        FROM Posts p
        JOIN Friends f
        ON ( (f.requester = ? AND f.requested = p.username) OR (f.requester = p.username AND f.requested = ?) )
        WHERE f.status = 'accepted' AND f.friendship_type = 'close' AND p.privacy = 'close';
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, SELECT_PRIVATE_POSTS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);

        result.clear();

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const char *post_username = (const char *)(sqlite3_column_text(stmt, 0));
            const char *content = (const char *)(sqlite3_column_text(stmt, 1));
            const char *time = (const char *)(sqlite3_column_text(stmt, 2));

            if (post_username != nullptr && content != nullptr && time != nullptr)
            {
                result += post_username;
                result += "~";
                result += content;
                result += "~";
                result += time;
                result += "~";
            }
        }

        if (rc != SQLITE_DONE)
        {
            cerr << "Error stepping through rows: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}

bool DB_UPDATE_USERNAME(sqlite3 *db, char *db_error, const char *old_username, const char *new_username) {
    const char *UPDATE_USERNAME_USERS = R"(
        UPDATE Users
        SET username = ?
        WHERE username = ?;
    )";

    const char *UPDATE_USERNAME_FRIENDS = R"(
        UPDATE Friends
        SET requester = CASE WHEN requester = ? THEN ? ELSE requester END,
            requested = CASE WHEN requested = ? THEN ? ELSE requested END;
    )";

    const char *UPDATE_USERNAME_POSTS = R"(
        UPDATE Posts
        SET username = ?
        WHERE username = ?;
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, UPDATE_USERNAME_USERS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Error preparing update statement for Users: " << sqlite3_errmsg(db) << endl;
            return false;
        }
        sqlite3_bind_text(stmt, 1, new_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, old_username, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            cerr << "Error executing update statement for Users: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        rc = sqlite3_prepare_v2(db, UPDATE_USERNAME_FRIENDS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Error preparing update statement for Friends: " << sqlite3_errmsg(db) << endl;
            return false;
        }
        sqlite3_bind_text(stmt, 1, old_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, new_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, old_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, new_username, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            cerr << "Error executing update statement for Friends: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        rc = sqlite3_prepare_v2(db, UPDATE_USERNAME_POSTS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Error preparing update statement for Posts: " << sqlite3_errmsg(db) << endl;
            return false;
        }
        sqlite3_bind_text(stmt, 1, new_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, old_username, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            cerr << "Error executing update statement for Posts: " << sqlite3_errmsg(db) << endl;
            return false;
        }
    }

    return true;
}



bool DB_UPDATE_PASSWORD(sqlite3 *db, char *db_error, const char *username, const char *new_password) {
    const char *UPDATE_PASSWORD = R"(
        UPDATE Users
        SET password = ?
        WHERE username = ?;
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, UPDATE_PASSWORD, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Error preparing update statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, new_password, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            cerr << "Error executing update statement: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}

bool DB_GET_PRIVACY(sqlite3 *db, char *db_error, const char *username, string &privacy)
{
    const char *SELECT_PRIVACY = R"(
        SELECT profile_privacy
        FROM Users
        WHERE username = ?;
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex); 

        rc = sqlite3_prepare_v2(db, SELECT_PRIVACY, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {
            const char *privacy_value = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            if (privacy_value != nullptr)
            {
                privacy = privacy_value;
            }
            else
            {
                cerr << "Error: Privacy value is null for user: " << username << endl;
                sqlite3_finalize(stmt);
                return false;
            }
        }
        else if (rc == SQLITE_DONE)
        {
            cerr << "No privacy setting found for user: " << username << endl;
            sqlite3_finalize(stmt);
            return false;
        }
        else
        {
            cerr << "Error executing SELECT statement: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt); 
    }

    return true;
}

bool DB_SET_PRIVACY(sqlite3 *db, char *db_error, const char *username, const char *privacy)
{
    const char *UPDATE_PRIVACY = R"(
        UPDATE Users
        SET profile_privacy = ?
        WHERE username = ?;
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex); 

        rc = sqlite3_prepare_v2(db, UPDATE_PRIVACY, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing UPDATE statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, privacy, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "Error executing UPDATE statement: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt); 
    }

    return true;
}


bool DB_MAKE_GROUP(sqlite3 *db, char *db_error, const string &group_name, const string &people)
{
    const char *INSERT_GROUP = R"(
        INSERT INTO Groups (group_name, users) VALUES (?, ?);
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, INSERT_GROUP, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing INSERT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        rc = sqlite3_bind_text(stmt, 1, group_name.c_str(), -1, SQLITE_STATIC);
        if (rc != SQLITE_OK)
        {
            cerr << "Error binding group_name: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        rc = sqlite3_bind_text(stmt, 2, people.c_str(), -1, SQLITE_STATIC);
        if (rc != SQLITE_OK)
        {
            cerr << "Error binding users: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "Error executing INSERT: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}


bool DB_GET_MY_GROUPS(sqlite3 *db, char *db_error, const string &username, string &groups)
{
    const char *SELECT_MY_GROUPS = R"(
        SELECT group_id, group_name
        FROM Groups
        WHERE users LIKE ? OR users LIKE ? OR users LIKE ? OR users = ?;
    )";

    sqlite3_stmt *stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, SELECT_MY_GROUPS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing SELECT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        string like_start = username + ",%";
        string like_middle = "%," + username + ",%";
        string like_end = "%," + username;
        string exact_match = username;

        sqlite3_bind_text(stmt, 1, like_start.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, like_middle.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, like_end.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, exact_match.c_str(), -1, SQLITE_STATIC);

        groups.clear();

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const char *group_id = (const char *)(sqlite3_column_text(stmt, 0));
            const char *group_name = (const char *)(sqlite3_column_text(stmt, 1));

            if (group_id != nullptr && group_name != nullptr)
            {
                groups += " ";
                groups += group_id;
                groups += " ";
                groups += group_name;
            }
        }

        if (rc != SQLITE_DONE)
        {
            cerr << "Error stepping through rows: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}

bool DB_INSERT_MESSAGE(sqlite3* db, char* db_error, const char* username, int group_id, const char* content)
{
    const char* INSERT_MESSAGE_SQL = R"(
        INSERT INTO Messages (group_id, username, content)
        VALUES (?, ?, ?);
    )";

    sqlite3_stmt* stmt;
    int rc;

    {
        lock_guard<mutex> lock(db_mutex);

        rc = sqlite3_prepare_v2(db, INSERT_MESSAGE_SQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "Error preparing INSERT statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, group_id);         
        sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC); 

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "Error executing INSERT statement: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    return true;
}



bool DB_GET_MESSAGES(sqlite3* db, char* db_error, int group_id, string& messages) {
    if (group_id <= 0) {
        cerr << "Invalid group_id provided to DB_GET_MESSAGES" << endl;
        return false;
    }

    const char* GET_MESSAGES_SQL = R"(
        SELECT username, created_at, content
        FROM Messages
        WHERE group_id = ?;
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, GET_MESSAGES_SQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    sqlite3_bind_int(stmt, 1, group_id);

    messages.clear();

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        if (username && created_at && content) {
            messages += "~"+string(username) + "~" + string(created_at) + "~" + string(content);
        }
    }

    if (rc != SQLITE_DONE) {
        cerr << "Error stepping through result set: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

