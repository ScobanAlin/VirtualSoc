#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>
#include "data_base.h"
using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024

sqlite3* db;
char* db_error=nullptr;

int server_socket;
struct sockaddr_in server_address;

string admin_password="alinesmecher";

struct client_data
{
    int client_socket;
    sockaddr_in cl;
};
bool refresh = false;
vector<string> active_users;

void *handle_client(void *arg)
{
    client_data *data = (client_data *)arg;
    int client_socket = data->client_socket;
    char buffer[BUFFER_SIZE];

    while (true)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int len=recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (len <= 0)
        {
            if (len == 0) {
                cout << "Client disconnected" << endl;
            } else {
                cout << "recv error" << endl;
            }
            break;  
        }

        if(strncmp(buffer, "get_messages", 12) != 0 and  strncmp(buffer, "get_my_groups", 11) != 0 and  strncmp(buffer, "get_privacy", 11) != 0 and  strncmp(buffer, "get_close_feed", 14) != 0 and strncmp(buffer, "get_friends_feed", 16) != 0 and strncmp(buffer, "get_my_friends", 14) != 0 and strncmp(buffer, "get_friend_requests", 19) != 0 and strncmp(buffer, "get_public_feed", 15) != 0 and strncmp(buffer, "get_active_users", 16) != 0)
        {
            cout<<buffer<<endl;
        }

        if (strncmp(buffer, "login", 5) == 0)   ///DONE
        {

            char *command = strtok(buffer," ");
            char *username = strtok(nullptr," ");
            char *password = strtok(nullptr," "); 
            bool logged_in=false;
            for(auto &it:active_users)
            {
                if(strcmp(it.c_str(),username) == 0)
                {
                    logged_in = true;
                }
            }
            if(logged_in == false)
            {
                if(DB_LOGIN_USER(db,db_error,username,password) == true)
                {
                    active_users.push_back(username);
                    send(client_socket, "LOGIN_OK", 8, 0);
                }
                else 
                {
                    send(client_socket, "NOT_LOGIN_OK", 8, 0);
                }
            }
            else
            {
                send(client_socket, "NOT_LOGIN_OK", 8, 0);

            }

        }
        else if(strncmp(buffer,"check_user",10) == 0)
        {
            char* command= strtok(buffer," ");
            char *my_username=strtok(nullptr," ");
            char *username= strtok(nullptr," ");

            if(DB_CHECK_USER(db,db_error,username) == false and DB_CHECK_FRIENDSHIP(db,db_error,username,my_username) == true)
            {
                send(client_socket,"CHECK_USER_OK",13,0);
            }
            else 
            {
                send(client_socket,"NOT_CHECK_USER_OK",17,0);
            }
        }
        else if (strncmp(buffer, "logout", 6) == 0)     ///DONE
        {
            char *received_username=strtok(buffer," ");
            received_username=strtok(nullptr," ");
            for (int i = 0; i < active_users.size(); i++) {
                if (active_users[i] == received_username) {
                    active_users.erase(active_users.begin() + i);
                    break;
                }
            }
            send(client_socket, "LOGOUT_OK", 9, 0);
        }
        else if (strncmp(buffer, "register", 8) == 0)    ///DONE
        {
            char *command=strtok(buffer," ");
            char* username=strtok(nullptr," ");
            char *password=strtok(nullptr," ");
            if(DB_REGISTER_USER(db,db_error,username,password) == true)
            {
                send(client_socket, "REGISTER_OK", 11, 0);
            }
            else
            {
                send(client_socket, "NOT_REGISTER_OK", 15, 0);
            }

        }
        else if (strncmp(buffer, "continue_without_login", 22) == 0)
        {
            send(client_socket, "CONTINUE_WITHOUT_LOGIN_OK", 25, 0);
        }
        else if (strncmp(buffer, "post", 4) == 0)
        {
            char *command = strtok(buffer,"~");
            char *username = strtok(nullptr,"~");
            char *content = strtok(nullptr,"~");
            char *privacy = strtok(nullptr,"~");

            if(DB_POST_INSTERT(db,db_error,username,content,privacy) == true)
            {
                send(client_socket, "POST_OK", 7, 0);
            }
            else
            {
                send(client_socket, "NOT_POST_OK", 11, 0);
            }


        }
        else if (strncmp(buffer, "add_friend", 10) == 0)
        {
            char *command = strtok(buffer," ");
            char *user_that_requested = strtok(nullptr," ");
            char *user_requested = strtok(nullptr," ");
            char *type = strtok(nullptr," ");

            if(DB_ADD_FRIEND(db,db_error,user_that_requested,user_requested,type) == true)
            {
                send(client_socket, "ADD_FRIEND_OK", 13, 0);        
            }
            else
            {
                send(client_socket,"NOT_ADD_FRIEND_OK",17,0);
            }
        }
        else if (strncmp(buffer, "get_public_feed", 15) == 0)   ///DONE
        {
            string message;
            if(DB_GET_PUBLIC_POSTS(db,db_error,message) == true)
            {
                message = "GET_PUBLIC_FEED_OK~"+message;
                send(client_socket, message.c_str(), message.size(), 0);
            }
            else
            {
                send(client_socket, "NOT_GET_PUBLIC_FEED_OK", 22, 0);
            }
        }
        else if (strncmp(buffer, "get_friend_requests", 19) == 0)
        {
            char *command = strtok(buffer, " ");
            char *username = strtok(nullptr, " ");
            string message;
            if (DB_GET_FRIEND_REQUESTS(db, db_error, username, message) == true)
            {
                message = "GET_FRIEND_REQUESTS_OK" + message;
                send(client_socket, message.c_str(), message.size(), 0);
            }
            else
            {
                send(client_socket, "NOT_GET_FRIEND_REQUESTS_OK", 22, 0);
            }
        }
        else if (strncmp(buffer, "get_friends_feed", 15) == 0)
        {
            char *command = strtok(buffer," ");
            char *username = strtok(nullptr, " ");
            string message;
            if(DB_GET_FRIEND_POSTS(db,db_error,username,message) == true)
            {
                message = "GET_FRIENDS_FEED_OK~"+message;
                send(client_socket, message.c_str(), message.size(), 0);
            }
            else
            {
                send(client_socket, "NOT_GET_FRIENDS_FEED_OK", 22, 0);
            }
        }
        else if (strncmp(buffer, "get_close_feed", 14) == 0)
        {
            char *command = strtok(buffer," ");
            char *username = strtok(nullptr, " ");
            string message;
            if(DB_GET_CLOSE_POSTS(db,db_error,username,message) == true)
            {
                message = "GET_CLOSE_FEED_OK~"+message;
                send(client_socket, message.c_str(), message.size(), 0);
            }
            else
            {
                send(client_socket, "NOT_GET_CLOSE_FEED_OK", 22, 0);
            }
        }
        else if(strncmp(buffer,"set_privacy",11) == 0)
        {
            char* command=strtok(buffer," ");
            char* username = strtok(nullptr," ");
            char* privacy = strtok(nullptr," ");

            if(DB_SET_PRIVACY(db,db_error,username,privacy) == true)
            {
                send(client_socket,"SET_PRIVACY_OK",14,0);
            }
            else send(client_socket,"NOT_SET_PRIVACY_OK",18,0);
        }
        else if(strncmp(buffer,"get_privacy",11) == 0)
        {
            char* command=strtok(buffer," ");
            char* username = strtok(nullptr," ");
            string privacy;
            if(DB_GET_PRIVACY(db,db_error,username,privacy) == true)
            {
                privacy= "GET_PRIVACY_OK " + privacy;
                send(client_socket,privacy.c_str(),privacy.size(),0);
            }
            else
            {
                send(client_socket,"NOT_GET_PRIVACY_OK",18,0);
            }

        }
        else if(strncmp(buffer,"log_as_admin",12) == 0)
        {
            char *password=strtok(buffer," ");
            password=strtok(nullptr," ");
            if(strcmp(password,admin_password.c_str()) == 0)
            {
                send(client_socket,"LOG_AS_ADMIN_OK",15,0);
            }
            else 
            {
                send(client_socket,"Incorrect Password",18,0);
            }
        }
        else if (strncmp(buffer, "get_active_users", 16) == 0)
        {

            string message_to_send = "GET_ACTIVE_USERS_OK\n";
            for (auto &user : active_users)
            {
                message_to_send += user + "\n";
            }
            send(client_socket, message_to_send.c_str(), message_to_send.size(), 0);
        }
        else if (strncmp(buffer, "get_my_friends", 14) == 0)
        {
            char *command = strtok(buffer, " ");
            char *username = strtok(nullptr, " ");
            string message;
            if (DB_GET_MY_FRIENDS(db, db_error, username, message) == true)
            {
                message = "GET_MY_FRIENDS_OK" + message;
                send(client_socket, message.c_str(), message.size(), 0);
            }
            else
            {
                send(client_socket, "NOT_GET_MY_FRIENDS_OK", 22, 0);
            }
        }
        else if(strncmp(buffer, "accept_friend", 13) == 0)
        {
            char *command =strtok(buffer," " );
            char *user1 = strtok(nullptr, " ");
            char *user2 = strtok(nullptr, " ");
            if(DB_ACCEPT_FRIENDSHIP(db,db_error,user1,user2) == true)
            {
                send(client_socket,"ACCEPT_FRIEND_OK",16,0);
            }
            else 
            {
                send(client_socket,"NOT_ACCEPT_FRIEND_OK",20,0);
            }
        }  
        else if(strncmp(buffer, "reject_friend", 13) == 0)
        {
            char *command =strtok(buffer," " );
            char *user1 = strtok(nullptr, " ");
            char *user2 = strtok(nullptr, " ");
            if(DB_REJECT_FRIENDSHIP(db,db_error,user1,user2) == true)
            {
                send(client_socket,"REJECT_FRIEND_OK",16,0);
            }
            else 
            {
                send(client_socket,"NOT_REJECT_FRIEND_OK",20,0);
            }
        }
        else if(strncmp(buffer,"delete_friend",13) == 0)
        {
            char *command =strtok(buffer," " );
            char *user1 = strtok(nullptr, " ");
            char *user2 = strtok(nullptr, " ");
            if(DB_DELETE_FRIENDSHIP(db,db_error,user1,user2) == true)
            {
                send(client_socket,"DELETE_FRIEND_OK",16,0);
            }
            else 
            {
                send(client_socket,"NOT_DELETE_FRIEND_OK",20,0);
            }
        }
        else if(strncmp(buffer,"update_username",15) == 0)
        {
            char *command = strtok(buffer, " ");
            char *username = strtok(nullptr, " ");
            char *new_username = strtok(nullptr, " ");
            if (DB_UPDATE_USERNAME(db, db_error, username, new_username) == true)
            {
                for (auto &it : active_users) 
                {
                    if (it == username)
                    {
                        it = new_username; 
                        break;             
                    }
                }
                send(client_socket, "UPDATE_USERNAME_OK", 18, 0);
            }
            else
            {
                send(client_socket, "NOT_UPDATE_USERNAME_OK", 22, 0);
            }
        }
        else if (strncmp(buffer, "update_password", 15) == 0)
        {
            char *command =strtok(buffer," " );
            char *username = strtok(nullptr, " ");
            char *new_password = strtok(nullptr, " ");
            if(DB_UPDATE_PASSWORD(db,db_error,username,new_password) == true)
            {
                send(client_socket,"UPDATE_PASSWORD_OK",18,0);
            }
            else 
            {
                send(client_socket,"NOT_UPDATE_PASSWORD_OK",22,0);
            }
        }
        else if (strncmp(buffer, "make_group", 10) == 0)
        {
            char *command = strtok(buffer," ");
            char *group_name = strtok(nullptr," ");
            char *people = strtok(nullptr," ");

            if(DB_MAKE_GROUP(db,db_error,group_name,people) == true)
            {
                send(client_socket,"MAKE_GROUP_OK",13,0);
            }
            else 
            {
                send(client_socket," NOT_MAKE_GROUP_OK",17,0);

            }
        }
        else if(strncmp(buffer,"get_my_groups",13) == 0)
        {
            char *command= strtok(buffer," ");
            char *username= strtok(nullptr," ");
            string groups;
            if(DB_GET_MY_GROUPS(db,db_error,username,groups) == true)
            {
                groups = "GET_MY_GROUPS_OK" +groups;
                send(client_socket,groups.c_str(),groups.size(),0);
            }
            else
            {
                send(client_socket," NOT_GET_MY_GROUPS_OK",20,0);

            } 

        }
        else if(strncmp(buffer,"get_messages",12) == 0)
        {
            char* command=strtok(buffer," ");
            char* gr =strtok(nullptr," ");
            int group_id=atoi(gr);
            string messages;
            if(DB_GET_MESSAGES(db,db_error,group_id,messages) == true)
            {
                string msg = "GET_MESSAGES_OK" + messages;
                send(client_socket,msg.c_str(),msg.size(),0);
            }
            else 
            {
                send(client_socket,"NOT_GET_MESSAGES_OK",19,0);
            }
        }
        else if(strncmp(buffer,"send_message",12) == 0)
        {
            char* command= strtok(buffer,"~");
            char* username = strtok(nullptr,"~");
            char* gr = strtok(nullptr,"~");
            char* content = strtok(nullptr,"~");
            int group_id = atoi(gr);
            if(DB_INSERT_MESSAGE(db,db_error,username,group_id,content) == true)
            {
                send(client_socket,"SEND_MESSAGE_OK",15,0);
            }
            else
            {
                send(client_socket,"NOT_SEND_MESSAGE_OK",19,0);
            }


        }
        else
        {
            send(client_socket, "Unknown command", 16, 0);
        }
    }

    close(client_socket);
    delete data;
    pthread_exit(nullptr);
}





int main()
{

    DB_SETUP(db,db_error);
    


    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    listen(server_socket, 5);


    cout << "SERVER STARTED" << endl;

    while (true)
    {
        sockaddr_in cl;
        socklen_t cl_size = sizeof(cl);
        int client_socket = accept(server_socket, (struct sockaddr *)&cl, &cl_size);

        pthread_t client_thread;
        client_data *data = new client_data;
        data->client_socket = client_socket;
        data->cl = cl;

        if (pthread_create(&client_thread, nullptr, handle_client, (void *)data) != 0)
        {
            cout << "Could not create thread" << endl;
            close(client_socket);
            delete data;
        }
        else
        {
            pthread_detach(client_thread); 
        }
    }

    close(server_socket);
    return 0;
}
