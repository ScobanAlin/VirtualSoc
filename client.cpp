#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_sdl2.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <vector>
#include <stack>
#include <mutex>
////
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
////
#include <chrono>
using namespace std;
using namespace ImGui;


////
//////////////////////////   ####   SERVER-CLIENT COMM   ###     /////////////////////////////////////
#define PORT 8080
#define BUFFER_SIZE 1024

std::mutex mutex;

int socket_client = -1;
struct sockaddr_in server;
////////////////////////////    ### ERRORS-Popup   ###    ////////////////////////////////
struct ErrMsg
{
    string text;
    chrono::steady_clock::time_point time;
} error;

bool admin_status=false;

/////////////////////////////
void push_error(const string &message)
{
    error.text = message;
    error.time = chrono::steady_clock::now();
}

void render_error()
{

    const float display_time = 2.0f;
    float time_elapsed = chrono::duration<float>(chrono::steady_clock::now() - error.time).count();

    if (display_time > time_elapsed)
    {
        ImVec2 window_size = GetIO().DisplaySize;
        ImVec2 popup_pos = ImVec2(window_size.x - 10, window_size.y - 30.0f);

        SetNextWindowPos(popup_pos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        SetNextWindowSize(ImVec2(280, 30));

        PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
        PushStyleColor(ImGuiCol_Border, IM_COL32(255, 0, 0, 200));

        Begin(" ", nullptr,
              ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);

        TextUnformatted(error.text.c_str());

        End();
        PopStyleColor(2);
    }
}
///////////////////////

bool connect_to_server(const char *ip, int port)
{
    socket_client = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (connect(socket_client, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("Eroare la connect().\n");
        return false;
    }


    return true;
}

void disconnect_from_server()
{
    if (socket_client != -1)
    {
        close(socket_client);
    }
}

bool send_to_server(const char *message)
{
    if (socket_client < 0)
    {
        printf("Not connected to the server.\n");
        return false;
    }

    if (send(socket_client, message, strlen(message), 0) < 0)
    {
        perror("Send failed");
        return false;
    }
    return true;
}

bool receive_from_server(char *buffer, size_t size)
{
    if (socket_client < 0)
    {
        printf("Not connected to the server.\n");
        return false;
    }

    int length = recv(socket_client, buffer, size - 1, 0);
    if (length < 0)
    {
        perror("Receive failed");
        return false;
    }

    buffer[length] = '\0';
    return true;
}

/////////////////////////////////////////////////////////////////////////

ImFont *font_big = nullptr;
ImFont *font_medium = nullptr;
ImFont *font_small = nullptr;

enum Page
{
    LOGIN,
    REGISTER,
    HOME,
    POSTS,
    FRIENDS,
    CLOSE,
    PROFILE,
    FRIEND_POSTS,
    CONSOLE,
    MESSAGES,
};
int group_count=0;
Page current_page = Page::LOGIN;
bool status = false;

vector<string> active_users;
struct PUBLIC_POST{
    string username;
    string content;
    string time;
};
vector<PUBLIC_POST> public_feed;


struct FRIEND_POST
{
    string username;
    string content;
    string time;
};
vector<FRIEND_POST> friends_feed;

struct CLOSE_POST
{
    string username;
    string content;
    string time;
};
vector<CLOSE_POST> close_feed;

struct FRIEND_REQUEST
{
    string name;
    string type;

};

struct FRIEND
{
    string name;
    string type;
};

struct CURRENT_USER{
    string username;
    vector<FRIEND> friends;
    vector<FRIEND_REQUEST> friend_requests;
    string privacy;
};
CURRENT_USER current_user;

struct Message
{
    string username;
    string time;
    string content;
};

struct group
{
    string id;
    string name;
    vector<Message> messages;
};

group current_group;

vector<group> groups;
    
//////////////////////////   ####   IMGUI    ###     /////////////////////////////////////

void load_active_users()
{

    for(auto &user:active_users)
    {
        Text(user.c_str());
        if(user == current_user.username)
        {
            SameLine();
            Text("(you)");
        }
    }
}

void HOME_ACTIVE_USERS()
{
    ImVec2 window_size(200, 500);
    ImVec2 window_pos((800 - window_size.x) * 0.05, (600 - window_size.y) * 0.8f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin("Active Users", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    Text("Active Users");
    SeparatorText("");
    load_active_users();

    End();
}

static bool is_privacy_initialized = false;
static int privacy=0;

void HOME_MAIN()
{
    ImVec2 screen_size = GetIO().DisplaySize;
    ImVec2 window_size(530, 500);
    ImVec2 window_pos(screen_size.x * 0.3f, (600 - window_size.y) * 0.8f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f));
    Begin("Posts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    static char buffer[BUFFER_SIZE] = "\0";


    Text("Create a post");
    SeparatorText("");
    InputTextMultiline("##postContent", buffer, IM_ARRAYSIZE(buffer), ImVec2(-FLT_MIN, GetTextLineHeight() * 10), ImGuiInputTextFlags_AllowTabInput);
    if (!is_privacy_initialized) {
    if (current_user.privacy == "private") {
        privacy = 1;
    } else if (current_user.privacy == "public") {
        privacy = 0;
    }
    is_privacy_initialized = true;
}
    RadioButton("Public", &privacy, 0);
    SameLine();
    RadioButton("Private", &privacy, 1);
    SameLine();
    RadioButton("Close", &privacy, 2);
    if (Button("Post"))
    {
        if (status == false)
        {
            push_error("You must be logged in to post.");
        }
        else if (strlen(buffer) == 0)
        {
            push_error("Post must not be empty");
        }
        else
        {
            char priv[20];
            if (privacy == 0)
                strcpy(priv, "public");
            else if (privacy == 1)
                strcpy(priv, "private");
            else
                strcpy(priv, "close");
            char command[BUFFER_SIZE] = "\0";
            snprintf(command, sizeof(command), "post~%s~%s~%s", current_user.username.c_str(), buffer, priv);            if (send_to_server(command) == true)
            {
                buffer[0] = '\0';
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)) == true)
                {
                    //cout<<received_msg<<endl;
                    if (strcmp(received_msg, "POST_OK") == 0)
                    {
                        printf("%s\n",received_msg);
                    }
                }
            }
        }
    }

    SeparatorText("");

    End();
}

void FRIENDS_FRIENDS()
{
    ImVec2 window_size(200, 250);
    ImVec2 window_pos((800 - window_size.x) * 0.05, (600 - window_size.y) * 0.23f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin("My Friends", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    Text("My Friends");
    SeparatorText("");

    int index=0;
    for(const auto &it:current_user.friends)
    {
        string name="unfriend##" + to_string(index); 
        string nume = it.name + "(" + it.type + ")";
        Text(nume.c_str());
        SameLine();
        if(Button(name.c_str()))
        {
            string command = "delete_friend " + current_user.username + " " + it.name;
            if(send_to_server(command.c_str()) == true )
            {
                char received_msg[BUFFER_SIZE];
                if(receive_from_server(received_msg,sizeof(received_msg)) == true)
                {
                    if(strcmp(received_msg,"DELETE_FRIEND_OK") == 0)
                    {

                    }
                    else
                    {
                        push_error("Could not delete friend");
                    }
                }
            } 
        }
        index++;
    }

    End();
}

void FRIENDS_SEARCH_FRIENDS()
{
    ImVec2 screen_size = GetIO().DisplaySize;
    ImVec2 window_size(530, 500);
    ImVec2 window_pos(screen_size.x * 0.3f, (600 - window_size.y) * 0.8f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f));
    Begin("Search for Friends", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    static char friend_username[BUFFER_SIZE] = "\0";
    static int friend_type = 1;
    string fr_type;

    Text("Add a friend");
    SeparatorText("");
    InputText("##postContent", friend_username, IM_ARRAYSIZE(friend_username));
    RadioButton("Normal", &friend_type, 1);
    SameLine();
    RadioButton("Close", &friend_type, 2);
    Spacing();
    Spacing();
    if (Button("Add Friend"))
    {
        if(strstr(friend_username," "))
        {
            push_error("Friend Username can not include spaces.");
        }
        else if (strlen(friend_username) == 0)
        {
            push_error("Friend username must not be empty");
        }
        else
        {

            if (friend_type == 1)
            {
                fr_type = "normal";
            }
            else
            {
                fr_type = "close";
            }

            char command[BUFFER_SIZE];
            snprintf(command, sizeof(command), "add_friend %s %s %s", current_user.username.c_str(), friend_username,fr_type.c_str());
            if(send_to_server(command) == true)
            {
                friend_username[0] = '\0';
                char received_msg[BUFFER_SIZE];
                if(receive_from_server(received_msg,sizeof(received_msg)) == true)
                {
                    if(strcmp(received_msg , "ADD_FRIEND_OK") == 0)
                    {
                        printf("%s\n",received_msg);
                    }
                    else push_error("Invalid user.");
                }
            }
        }
        friend_username[0] = '\0';

    }

    SeparatorText("");
    Spacing();

    End();
}


void FRIENDS_FRIEND_REQUESTS()
{
    ImVec2 window_size(200, 245);
    ImVec2 window_pos((800 - window_size.x) * 0.05, (600 - window_size.y) * 0.945f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin("Friend Requests", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    Text("Friend Requests");
    SeparatorText("");

    int index = 0; 
    for (const auto &it : current_user.friend_requests)
    {
        string nume = it.name+"(" + it.type + ")";
        Text(nume.c_str());
        SameLine();

        string accept_button_id = "accept##" + to_string(index);
        if (Button(accept_button_id.c_str()))
        {
            string command = "accept_friend " + current_user.username + " " + it.name;
            if (send_to_server(command.c_str()) == true)
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)) == true)
                {
                    printf("%s\n", received_msg);
                    if (strncmp(received_msg, "ACCEPT_FRIEND_OK", 16) == 0)
                    {
                    }
                    else
                    {
                        push_error("Could not load accept friendship.");
                    }
                }
            }
        }

        SameLine();

        string reject_button_id = "reject##" + std::to_string(index);
        if (Button(reject_button_id.c_str()))
        {
            string command = "reject_friend " + current_user.username + " " + it.name;
            if (send_to_server(command.c_str()) == true)
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)) == true)
                {
                    printf("%s\n", received_msg);
                    if (strncmp(received_msg, "REJECT_FRIEND_OK", 16) == 0)
                    {
                        send_to_server("");
                    }
                    else
                    {
                        push_error("Could not load reject friendship.");
                    }
                }
            }
        }

        index++; 
    }

    End();
}

void POSTS_FEED()
{
    ImVec2 window_size(740, 500);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f+30);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f));
    Begin("Public Posts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    Text("Public Posts");
    SeparatorText("");
    
    for(auto &post:public_feed)
    {   
        Text(post.username.c_str());
        SameLine();
        Text(":");
        SameLine();
        Text(post.time.c_str());
        SameLine();
        Text("(UTC)");
        Text(post.content.c_str());
        Spacing();
        Separator();
    }

    End();
}

void FRIEND_POSTS_FEED()
{
    ImVec2 window_size(740, 500);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f+30);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f));
    Begin("Friend Posts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    Text("Friend's Posts");
    SeparatorText("");

    for(auto &post:friends_feed)
    {   
        Text(post.username.c_str());
        SameLine();
        Text(":");
        SameLine();
        Text(post.time.c_str());
        SameLine();
        Text("(UTC)");
        Text(post.content.c_str());
        Spacing();
        Separator();
    }

    End();
}

void CLOSE_FEED()
{
    ImVec2 window_size(740, 500);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f+30);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f));
    Begin("Close Friend's Posts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    Text("Close Friend's Posts");
    SeparatorText("");

    for(auto &post:close_feed)
    {   
        Text(post.username.c_str());
        SameLine();
        Text(":");
        SameLine();
        Text(post.time.c_str());
        SameLine();
        Text("(UTC)");
        Text(post.content.c_str());
        Spacing();
        Separator();
    }

    End();
}



void NAVBAR()
{
    if (Button("HOME"))
    {
        char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "get_active_users");

        if (send_to_server(command) == true)
        {
            char received_msg[BUFFER_SIZE];
            if (receive_from_server(received_msg, sizeof(received_msg)) == true)
            {
                printf("%s\n", received_msg);
                if (strncmp(received_msg, "GET_ACTIVE_USERS_OK", 19) == 0)
                {
                    char *name = strtok(received_msg, "\n");
                    name = strtok(nullptr, "\n");
                    active_users.clear();

                    while (name != nullptr)
                    {
                        active_users.push_back(name);
                        name = strtok(nullptr, "\n");
                    }

                    current_page = Page::HOME;
                }
                else
                {
                    push_error("Could not load active users.");
                }
            }
        }
    }
    SameLine();
    if (Button("POSTS"))
    {
        char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "get_public_feed");

        if(send_to_server(command) == true)
        {
            char received_msg[BUFFER_SIZE];
            if(receive_from_server(received_msg,sizeof(received_msg)) == true)
            {
                if(strncmp(received_msg,"GET_PUBLIC_FEED_OK",18) == 0)
                {
                    current_page = Page::POSTS;
                }
                else
                {
                    push_error("Could not load page.");
                }
            }
        }
    }
    SameLine();
    if (status == 1)
        if (Button("FRIEND'S POSTS"))
        {
            char command[BUFFER_SIZE];
            snprintf(command, sizeof(command), "get_friends_feed %s", current_user.username.c_str());

            if (send_to_server(command) == true)
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    cout << received_msg << endl;
                    if (strncmp(received_msg, "GET_FRIENDS_FEED_OK", 19) == 0)
                    {

                        friends_feed.clear();
                        char *command = strtok(received_msg, "~");
                        char *username = strtok(nullptr, "~");
                        while (username != nullptr)
                        {
                            char *content = strtok(nullptr, "~");
                            char *time = strtok(nullptr, "~");
                            FRIEND_POST post;
                            post.username = username;
                            post.content = content;
                            post.time = time;

                            friends_feed.push_back(post);
                            username = strtok(nullptr, "~");
                        }
                        current_page = Page::FRIEND_POSTS;
                    }
                }
            }
        }
    SameLine();
    if (status == 1)
        if (Button("CLOSE"))
        {
            char command[BUFFER_SIZE];
            snprintf(command, sizeof(command), "get_close_feed %s", current_user.username.c_str());

            if (send_to_server(command) == true)
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    cout<<received_msg<<endl;

                    if (strncmp(received_msg, "GET_CLOSE_FEED_OK", 17) == 0)
                    {
                        close_feed.clear();
                        char *command = strtok(received_msg, "~");
                        char *username = strtok(nullptr, "~");
                        while (username != nullptr)
                        {
                            char *content = strtok(nullptr, "~");
                            char *time = strtok(nullptr, "~");
                            CLOSE_POST post;
                            post.username = username;
                            post.content = content;
                            post.time = time;

                            close_feed.push_back(post);
                            username = strtok(nullptr, "~");
                        }
                        current_page = Page::CLOSE;

                    }
                }
                        
                        
                        
                        
            }
        }
    SameLine();
    if(status == 1)
    if (Button("FRIENDS"))
    {
        char command[BUFFER_SIZE];
        snprintf(command,sizeof(command),"get_my_friends %s",current_user.username.c_str());

        if(send_to_server(command) == true)
        {
            char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "GET_MY_FRIENDS_OK", 17) == 0)
                    {
                        char *flag = strtok(received_msg, " ");
                        char *friend_name = strtok(nullptr, " ");
                        char *type = strtok(nullptr, " ");
                        current_user.friends.clear();

                        while (friend_name != nullptr)
                        {
                            FRIEND fr;
                            fr.name = friend_name;
                            fr.type = type;
                            current_user.friends.push_back(fr);

                            friend_name = strtok(nullptr, " ");
                            type = strtok(nullptr, " ");
                        }
                    }
                    else
                    {
                        push_error("Could not load friends");
                    }
                }
        }

        current_page = Page::FRIENDS;
    }
    SameLine();
    if(status == 1)
    if (Button("PROFILE"))
    {
        current_page = Page::PROFILE;
    }
    SameLine();
    if(status == 1)
    if(Button("MESSAGES"))
    {
        current_page = Page::MESSAGES;
    }
    SameLine();
    if (status == false)
    {
        if (Button("LOGIN"))
        {
            current_page = Page::LOGIN;
        }
    }
    else
    {
        if (Button("LOGOUT"))
        {
            
            char command[BUFFER_SIZE];
            snprintf(command, sizeof(command), "logout %s", current_user.username.c_str());
            if (send_to_server(command) == true)
            {
                current_user.username.clear();
                char received_message[BUFFER_SIZE];
                if (receive_from_server(received_message, sizeof(received_message)) == true)
                {
                    printf("%s\n", received_message);
                    current_page = Page::LOGIN;
                    status = false;
                    char command[BUFFER_SIZE];
                    snprintf(command, sizeof(command), "get_active_users");

                    if (send_to_server(command) == true)
                    {
                        char received_msg[BUFFER_SIZE];
                        if (receive_from_server(received_msg, sizeof(received_msg)) == true)
                        {
                            printf("%s\n", received_msg);
                            if (strncmp(received_msg, "GET_ACTIVE_USERS_OK", 19) == 0)
                            {
                                char *name = strtok(received_msg, "\n");
                                name = strtok(nullptr, "\n");
                                active_users.clear();
                                current_group.id.clear();
                                current_group.name.clear();
                                current_group.messages.clear();
                                current_user.friend_requests.clear();
                                current_user.friends.clear();
                                current_user.privacy.clear();
                                current_user.username.clear();


                                while (name != nullptr)
                                {
                                    active_users.push_back(name);
                                    name = strtok(nullptr, "\n");
                                }

                            }
                            else
                            {
                                push_error("Could not load active users.");
                            }
                        }
                    }
                }
            }
        }
    }
    SeparatorText("");
}

void CONSOLE_MAIN()
{

    ImVec2 window_size1(740, 500);
    ImVec2 window_pos1((800 - window_size1.x) * 0.5f, (600 - window_size1.y) * 0.5f+30);
    SetNextWindowPos(window_pos1, ImGuiCond_Always);
    SetNextWindowSize(window_size1, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f));
    Begin("Console ", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove );
    PopStyleColor(1);

    Text("Console");
    SeparatorText("");

    if(admin_status == false)
    { 
        static char password[BUFFER_SIZE];

        static bool see_password = false;

        if (see_password == true)
        {
            InputText("Password", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_None);
        }
        else
        {
            InputText("Password", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_Password);
        }
        Checkbox("Show Password", &see_password);


        char command[BUFFER_SIZE];
        snprintf(command,sizeof(command),"log_as_admin %s",password);

        if(Button("Log in as admin"))
        {
            if(strlen(password)==0)
            {
                push_error("Password must not be empty");
            }
            else
            {
                if(send_to_server(command) == true)
                {
                    char received_msg[BUFFER_SIZE];
                    if(receive_from_server(received_msg,sizeof(received_msg)) == true)
                    {
                        cout<<received_msg<<endl;
                        if(strcmp(received_msg,"LOG_AS_ADMIN_OK") == 0)
                        {
                            password[0]='\0';
                            admin_status = true;
                        }
                        else
                        {
                            push_error(received_msg);
                        }
                    }
                }
            }
        }
    }
    else
    {
        Text("Logged in as ADMIN");
        static char command[BUFFER_SIZE];
        InputText("COMMAND",command,sizeof(command));
        if(Button("Send Command"))
        {
            if(strlen(command)==0)
            {
                push_error("Password must not be empty");
            }
            else
            {
                if(send_to_server(command) == true)
                {
                    char received_msg[BUFFER_SIZE];



                    if(receive_from_server(received_msg,sizeof(received_msg)) == true)
                    {
                        cout<<received_msg<<endl;
                        if(strcmp(received_msg,"Unknown command") == 0)
                        {
                            push_error(received_msg);
                        }
                        command[0]='\0';
                    }
                }
            }
        }
    }
    

    

    
    End();
}

void render_CONSOLE()
{
    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Console";
    if (status == false)
    {
        strcat(page_name, " - ADMIN");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);

    if(Button("Back to Login"))
    {
        admin_status=false;
        current_page = Page::LOGIN;
    }

    CONSOLE_MAIN();

    End();
}

void render_LOGIN()
{
    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Login/Register";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));

    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);
    PushFont(font_big);
    Text("VirtualSoc");
    PopFont();

    SeparatorText("Login");

    Spacing();
    Spacing();

    static char username[20] = "\0";
    static char password[20] = "\0";
    static bool see_password = false;

    InputText("Username", username, IM_ARRAYSIZE(username));
    if (see_password == true)
    {
        InputText("Password", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_None);
    }
    else
    {
        InputText("Password", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_Password);
    }
    Checkbox("Show Password", &see_password);
    if (Button("Login"))
    {
        char command[BUFFER_SIZE];
        snprintf(command, BUFFER_SIZE, "login %s %s", username, password);
        if(strstr(username," "))
        {
            push_error("Username can not include spaces.");
        }
        else if(strstr(password," "))
        {
            push_error("Password can not include spaces.");
        }
        else if (strlen(username) == 0)
        {
            push_error("Username must not be empty");
        }
        else if (strlen(password) == 0)
        {
            push_error("Password must not be empty");
        }
        else if (send_to_server(command) == true)
        {

            char received_msg[BUFFER_SIZE];

            if (receive_from_server(received_msg, sizeof(received_msg)) == true)
            {
                if (strcmp(received_msg, "LOGIN_OK") == 0)
                {
                    current_user.username = username;
                    printf("%s\n", received_msg);
                    current_page = Page::HOME;
                    status = true;
                    username[0] = '\0';
                    password[0] = '\0';
                }
                else
                {
                    push_error("Username or password are incorrect");
                }
            }
        }
        if (status == true)
        {
            char command[BUFFER_SIZE];
            snprintf(command, sizeof(command), "get_active_users");

            if (send_to_server(command) == true)
            {
                char received_msg[BUFFER_SIZE];
                if(receive_from_server(received_msg,sizeof(received_msg)) == true)
                {
                    if (strncmp(received_msg, "GET_ACTIVE_USERS_OK",19) == 0)
                    {
                        char* command=strtok(received_msg,"\n");
                        char *name=strtok(nullptr,"\n");
                        active_users.clear();

                        while(name!=nullptr)
                        {
                            active_users.push_back(name);
                            name=strtok(nullptr,"\n");
                        }

                        printf("%s\n", received_msg);
                    }
                    else
                    {
                        push_error("Could not load active users");
                    }
                }
            }
        }
    }
    if (Button("Go To Register Page"))
    {

        current_page = Page::REGISTER;
        username[0] = '\0';
        password[0] = '\0';
    }

    SeparatorText("");

    if (Button("Continue Without Login"))
    {
        char command[BUFFER_SIZE] = "continue_without_login";
        if (send_to_server(command) == true)
        {
            char received_msg[BUFFER_SIZE];
            if (receive_from_server(received_msg, sizeof(received_msg)) == true)
            {
                if (strcmp(received_msg, "CONTINUE_WITHOUT_LOGIN_OK") == 0)
                {
                    printf("%s\n", received_msg);
                    current_page = Page::HOME;
                }
            }
        }
    }

    if(Button("Continue as Admin"))
    {
        current_page = Page::CONSOLE;
    }


    End();
}

void render_REGISTER()
{

    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Login/Register";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));

    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    PopStyleColor(1);

    Text("VirtualSoc");

    SeparatorText("Register");
    Spacing();
    Spacing();

    static char username[20] = "";
    static char password[20] = "";
    static bool see_password = false;
    InputText("Username", username, IM_ARRAYSIZE(username));

    if (see_password == true)
    {
        InputText("Password", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_None);
    }
    else
    {
        InputText("Password", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_Password);
    }

    Checkbox("Show Password", &see_password);

    if (Button("Register"))
    {
        char command[BUFFER_SIZE];
        snprintf(command, BUFFER_SIZE, "register %s %s", username, password);
        if(strstr(username," "))
        {
            push_error("Username can not include spaces.");
        }
        else if(strstr(password," "))
        {
            push_error("Password can not include spaces.");
        }
        else if (strlen(username) == 0)
        {
            push_error("Username must not be empty");
        }
        else if (strlen(password) == 0)
        {
            push_error("Password must not be empty");
        }
        else if (send_to_server(command) == true)
        {

            char received_msg[BUFFER_SIZE];

            if (receive_from_server(received_msg, sizeof(received_msg)) == true)
            {
                if (strcmp(received_msg, "REGISTER_OK") == 0)
                {
                    printf("%s\n", received_msg);
                    current_page = Page::LOGIN;
                }
                else
                {
                    push_error("Username already taken");
                }
            }
            username[0] = '\0';
            password[0] = '\0';
        }
    }

    if (Button("Go To Login Page"))
    {
        current_page = Page::LOGIN;
        username[0] = '\0';
        password[0] = '\0';
    }

    SeparatorText("");

    End();
}

void render_HOME()
{

    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Home";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);
    NAVBAR();

    HOME_ACTIVE_USERS();
    HOME_MAIN();

    End();
}

void render_POSTS()
{

    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Posts";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);
    NAVBAR();

    POSTS_FEED();

    End();
}

void render_FRIEND_POSTS()
{

    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Friend's Posts";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);
    NAVBAR();

    FRIEND_POSTS_FEED();

    End();
}

void render_CLOSE()
{

    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Close";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);
    NAVBAR();

    CLOSE_FEED();

    End();
}

void render_FRIENDS()
{

    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Friends";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);
    NAVBAR();

    FRIENDS_FRIENDS();
    FRIENDS_SEARCH_FRIENDS();
    FRIENDS_FRIEND_REQUESTS();
    End();
}

void PROFILE_MAIN()
{
    ImVec2 window_size(740, 500);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f+30);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f));
    Begin("Profile Settings", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    PopStyleColor(1);

    Text("Profile Settings");
    SeparatorText("");

    static char new_username[BUFFER_SIZE] = "";
    Text("Change your username.");
    Text("Current Username: ");
    SameLine();
    Text(current_user.username.c_str());
    InputText("Update Username", new_username, sizeof(new_username));
    if(Button("Update##username"))
    {
        if(strstr(new_username," "))
        {
            push_error("New Username can not include spaces.");
        }
        else
        {
            string command = "update_username "+ current_user.username + " " + new_username; 
            if(send_to_server(command.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if(receive_from_server(received_msg,sizeof(received_msg)))
                {
                    if(strcmp(received_msg,"UPDATE_USERNAME_OK") == 0)
                    {
                        current_user.username = new_username;
                    }
                    else 
                    {
                        push_error("Could not update username");
                    }
                }
            }
            new_username[0]='\0';

        }
    }

    SeparatorText("");

    static char new_password[BUFFER_SIZE] = "";
    static char confirm_password[BUFFER_SIZE]="";
    Text("Change your password.");
    InputText("Update Password", new_password, sizeof(new_password),ImGuiInputTextFlags_Password);
    InputText("Confirm Password", confirm_password, sizeof(confirm_password),ImGuiInputTextFlags_Password);

    if(Button("Update##password"))
    {
        if(strstr(new_username," "))
        {
            push_error("New Password can not include spaces.");
        }
        else if(strcmp(new_password,confirm_password) != 0)
        {
            push_error("Not the same password");
            new_password[0] = '\0';
            confirm_password[0] = '\0';
        }
        else
        {
            string command = "update_password " + current_user.username + " " + new_password;
            if (send_to_server(command.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strcmp(received_msg, "UPDATE_PASSWORD_OK") == 0)
                    {
                    }
                    else
                    {
                        push_error("Could not update password");
                    }
                }
            }
        }
        new_password[0] = '\0';
        confirm_password[0] = '\0';
    }

    SeparatorText("");

    static int privacy = 0;




    Text("Set Privacy");

    Text("Your privacy: ");
    SameLine();
    Text(current_user.privacy.c_str());

    RadioButton("Public", &privacy, 0);
    SameLine();
    RadioButton("Private", &privacy, 1);
    SameLine();
    
    if(Button("SET"))
    {
        is_privacy_initialized=false;
        char priv[20];
            if (privacy == 0)
                strcpy(priv, "public");
            else if (privacy == 1)
                strcpy(priv, "private");

            char command[BUFFER_SIZE] = "\0";
            snprintf(command, sizeof(command), "set_privacy %s %s ",current_user.username.c_str(), priv); // to_impl nume client
            if (send_to_server(command) == true)
            {
                command[0] = '\0';
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)) == true)
                {
                    if (strcmp(received_msg, "SET_PRIVACY_OK") == 0)
                    {
                        printf("%s\n",received_msg);
                    }
                }
            }
    }
    End();
}

void render_PROFILE()
{
    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Profile";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);
    NAVBAR();

    PROFILE_MAIN();

    End();
}

void MESSAGES_CHATS()
{
    ImVec2 window_size(200, 500);
    ImVec2 window_pos((800 - window_size.x) * 0.05, (600 - window_size.y) * 0.8f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin("Chats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    Text("Chats");
    SeparatorText("");
    
    for(auto &it:groups)
    {
        Text(it.name.c_str());
        string chat="Chat##"+it.id;

        if(Button(chat.c_str()))
        {
            current_group=it;
        }
        Separator();
    }

    End();
}

void MESSAGES_CREATE_GROUP()
{
    ImVec2 screen_size = GetIO().DisplaySize;
    ImVec2 window_size(530, 115);
    ImVec2 window_pos(screen_size.x * 0.3f, (600 - window_size.y) * 0.1f + 30);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f)); 
    Begin("Create a group", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    Text("Create a group");
    SeparatorText("");

    static char buffer[BUFFER_SIZE];  
    static char people[BUFFER_SIZE] = ""; 
    static char group_name[BUFFER_SIZE];
    InputText("Enter username##group", buffer, sizeof(buffer));
     SameLine();
    
    SameLine();
    if (Button("Add##group"))
    {
        if(strstr(buffer," "))
        {
            push_error("Username can not include spaces.");
        }
        else
        {
            string command = "check_user " + string(buffer) + " " + current_user.username;
            if (send_to_server(command.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "CHECK_USER_OK",13) == 0)
                    {

                        if (strlen(people) + strlen(buffer) + 1 < sizeof(people) and strcmp(buffer,current_user.username.c_str()) !=0)
                        {
                            if (group_count != 0)
                            {
                                strcat(people, ",");
                            }
                            group_count++;
                            strcat(people, buffer);
                        }
                    }
                    else
                    {
                        push_error("Not a valid username.");
                    }
                }
            }
        }

        buffer[0] = '\0';
    }

    
    InputText("Group name", group_name, sizeof(group_name));

    if (Button("Make Group"))
    {
        if(strstr(group_name," "))
        {
            push_error("Group name can not include spaces.");
        }
        else
        {
            string me= ","+current_user.username;
            strcat(people,me.c_str());
            if (strlen(people) > 0 and strlen(group_name) > 0)
            {
                string command = "make_group " + string(group_name) + " " + string(people);
                if(send_to_server(command.c_str()))
                {
                    char received_msg[BUFFER_SIZE];
                    if(receive_from_server(received_msg,sizeof(received_msg)))
                    {
                        if(strncmp(received_msg,"MAKE_GROUP_OK",13)  == 0)
                        {
                            
                        }
                        else
                        {
                            push_error("Could not make the group");
                        }

                    }
                }
                
            }
            else
            {
                push_error("Not enough users or no group name.");
            }
        }
        group_count = 0;
        group_name[0] = '\0';
        people[0] = '\0';
    }

    SameLine();
    Text("Added names: %s", people);

    End();
}


void MESSAGES_DISCUSSION()
{
    ImVec2 screen_size = GetIO().DisplaySize;
    ImVec2 window_size(530, 380);
    ImVec2 window_pos(screen_size.x * 0.3f, (600 - window_size.y) * 0.8f+20);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 255, 1.0f));
    Begin("Discussion", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing);
    PopStyleColor(1);

    static char buffer[BUFFER_SIZE] = "\0";


    Text("Discussion");
    SeparatorText("");
    
    if(current_group.name.size() > 0)
    {
        static char message_to_send[BUFFER_SIZE];
        Text("Group: %s",current_group.name.c_str());
        InputText("Write a message",message_to_send,sizeof(message_to_send));
        SameLine();
        if(Button("send"))
        {
            if(strlen(message_to_send) == 0)
            {
                push_error("Message can not be empty.");
            }
            else
            {
                string command = "send_message~"+ current_user.username + "~" + current_group.id + "~" + message_to_send;
                if(send_to_server(command.c_str()))
                {
                    char received_msg[BUFFER_SIZE];
                    if(receive_from_server(received_msg,sizeof(received_msg)))
                    {
                        if(strncmp(received_msg,"SEND_MESSAGE_OK",15) == 0 )
                        {

                        }
                        else 
                        {
                            push_error("Could not send the message.");
                        }
                    }
                }

            }
            message_to_send[0]='\0';
        }

        for(auto &msg:current_group.messages)
        {
            if(msg.username == current_user.username)
            {
                Text("Me");
            }
            else
            {
                Text(msg.username.c_str());
            }
            SameLine();
            Text("(%s) :",msg.time.c_str());
            SameLine();
            Text(msg.content.c_str());
        }

    }
    
    End();
}

void render_MESSAGES()
{
    ImVec2 window_size(800, 600);
    ImVec2 window_pos((800 - window_size.x) * 0.5f, (600 - window_size.y) * 0.5f);
    SetNextWindowPos(window_pos, ImGuiCond_Always);
    SetNextWindowSize(window_size, ImGuiCond_Always);

    char page_name[40] = "Home";
    if (status == false)
    {
        strcat(page_name, " - not logged in");
    }
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(50, 50, 50, 1.0f));
    Begin(page_name, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    PopStyleColor(1);
    NAVBAR();
    MESSAGES_CHATS();
    MESSAGES_CREATE_GROUP();
    MESSAGES_DISCUSSION();
    End();
}


void *refresh(void *arg)
{
    while (true)
    {
        string get_public_feed = "get_public_feed";
        if(current_page == Page::POSTS)
            if (send_to_server(get_public_feed.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "GET_PUBLIC_FEED_OK", 18) == 0)
                    {
                        public_feed.clear();
                        char *command = strtok(received_msg, "~");
                        char *username = strtok(nullptr, "~");
                        while (username != nullptr)
                        {
                            char *content = strtok(nullptr, "~");
                            char *time = strtok(nullptr, "~");
                            PUBLIC_POST post;
                            post.username = username;
                            post.content = content;
                            post.time = time;

                            public_feed.push_back(post);
                            username = strtok(nullptr, "~");
                        }
                    }
                }
            }
        string get_active_users = "get_active_users";
        string get_friends_feed = "get_friends_feed " + current_user.username ;
        string get_close_feed = "get_close_feed " + current_user.username;
        string get_my_friends = "get_my_friends " + current_user.username;
        string get_friend_requests = "get_friend_requests " + current_user.username;
        string get_privacy = "get_privacy " + current_user.username;
        string get_my_groups = "get_my_groups " + current_user.username;
        string get_messages = "get_messages " + current_group.id;
        if (status == true)
        {
            if(current_group.id.size()>0 and current_page == Page::MESSAGES)
            if(send_to_server(get_messages.c_str()))
            {
                char received_msg[BUFFER_SIZE*10];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if(strncmp(received_msg,"GET_MESSAGES_OK",15) == 0)
                    {
                        char *flag = strtok(received_msg,"~");
                        char* username =strtok(nullptr,"~");
                        current_group.messages.clear();
                        while(username != nullptr)
                        {
                            char* time =strtok(nullptr,"~");
                            char* content =strtok(nullptr,"~");
                            Message msg;
                            msg.username=username;
                            msg.time=time;
                            msg.content=content;
                            current_group.messages.push_back(msg);
                            username =strtok(nullptr,"~");
                            
                        }

                    }
                    else 
                    {
                        push_error("Could not load messages");
                    }
                }

            }
            if(current_page == Page::MESSAGES)
            if(send_to_server(get_my_groups.c_str()))
            {   
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "GET_MY_GROUPS_OK", 16) == 0)
                    {
                        char *command = strtok(received_msg, " ");
                        char *group_id = strtok(nullptr," ");
                        char *group_name = strtok(nullptr, " ");
                        groups.clear();

                        while(group_id != nullptr)
                        {
                            group new_group;
                            new_group.name=group_name;
                            new_group.id = group_id;
                            groups.push_back(new_group);
                            group_id = strtok(nullptr," ");
                            group_name = strtok(nullptr, " ");
                        }
                    }
                    else 
                    {
                        push_error("Could not load groups");
                    }
                }
            }
            if(current_page == Page::FRIEND_POSTS)
            if (send_to_server(get_friends_feed.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "GET_FRIENDS_FEED_OK", 19) == 0)
                    {
                        friends_feed.clear();
                        char *command = strtok(received_msg, "~");
                        char *username = strtok(nullptr, "~");
                        while (username != nullptr)
                        {
                            char *content = strtok(nullptr, "~");
                            char *time = strtok(nullptr, "~");
                            FRIEND_POST post;
                            post.username = username;
                            post.content = content;
                            post.time = time;

                            friends_feed.push_back(post);
                            username = strtok(nullptr, "~");
                        }
                    }
                }
            }
            if(current_page == Page::CLOSE)
            if (send_to_server(get_close_feed.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {

                    if (strncmp(received_msg, "GET_CLOSE_FEED_OK", 17) == 0)
                    {
                        close_feed.clear();
                        char *command = strtok(received_msg, "~");
                        char *username = strtok(nullptr, "~");
                        while (username != nullptr)
                        {
                            char *content = strtok(nullptr, "~");
                            char *time = strtok(nullptr, "~");
                            CLOSE_POST post;
                            post.username = username;
                            post.content = content;
                            post.time = time;

                            close_feed.push_back(post);
                            username = strtok(nullptr, "~");
                        }
                    }
                }
            }
            if(current_page == Page::HOME)
            if (send_to_server(get_active_users.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "GET_ACTIVE_USERS_OK", 19) == 0)
                    {

                        char *name = strtok(received_msg, "\n");
                        name = strtok(nullptr, "\n");

                        active_users.clear();
                        while (name != nullptr)
                        {
                            active_users.push_back(name);
                            name = strtok(nullptr, "\n");
                        }
                    }
                    else
                    {
                        push_error("Could not load active users");
                    }
                }
            }
            if(current_page == Page::FRIENDS)
            if (send_to_server(get_friend_requests.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "GET_FRIEND_REQUESTS_OK", 22) == 0)
                    {

                        char *name = strtok(received_msg, " ");
                        name = strtok(nullptr, " ");
                        char *type = strtok(nullptr, " ");

                        current_user.friend_requests.clear();
                        while (name != nullptr)
                        {
                            FRIEND_REQUEST fr;
                            fr.name = name;
                            fr.type = type;
                            current_user.friend_requests.push_back(fr);

                            name = strtok(nullptr, " ");
                            type = strtok(nullptr, " ");
                        }
                    }
                    else
                    {
                        push_error("Could not load friend requests");
                    }
                }
            }
            if (send_to_server(get_my_friends.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "GET_MY_FRIENDS_OK", 17) == 0)
                    {
                        char *flag = strtok(received_msg, " ");
                        char *friend_name = strtok(nullptr, " ");
                        char *type = strtok(nullptr, " ");
                        current_user.friends.clear();

                        while (friend_name != nullptr)
                        {
                            FRIEND fr;
                            fr.name = friend_name;
                            fr.type = type;
                            current_user.friends.push_back(fr);

                            friend_name = strtok(nullptr, " ");
                            type = strtok(nullptr, " ");
                        }
                    }
                    else
                    {
                        push_error("Could not load friends");
                    }
                }
            }
            if(current_page ==Page::HOME or current_page ==Page::PROFILE)
            if(send_to_server(get_privacy.c_str()))
            {
                char received_msg[BUFFER_SIZE];
                if (receive_from_server(received_msg, sizeof(received_msg)))
                {
                    if (strncmp(received_msg, "GET_PRIVACY_OK", 14) == 0)
                    {
                        char *flag = strtok(received_msg," ");
                        char *privacy = strtok(nullptr," ");
                        current_user.privacy = privacy;
                    }
                    else
                    {
                        push_error("Could not get user privacy");
                    }
                }

            }
        }
        sleep(1);
    }
}


int main(int argc, char **argv)
{

    if (!connect_to_server("127.0.0.1", PORT))
    {
        printf("Failed to connect to the server. Exiting...\n");
        return -1;
    }

    pthread_t update_thread;
    if (pthread_create(&update_thread, nullptr, refresh, nullptr) != 0)
    {
        std::cerr << "Error creating thread\n";
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_Window *window = SDL_CreateWindow("VirtualSoc",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,                               
                                          SDL_WINDOW_OPENGL);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    CreateContext();
    ImGuiIO &io = GetIO();
    (void)io;
    StyleColorsDark();

    // font_big = io.Fonts->AddFontFromFileTTF("/home/alin/Desktop/RC/VirtualSoc/fonts/font.ttf", 40.0f);
    // font_medium = io.Fonts->AddFontFromFileTTF("./fonts/font.ttf", 20.0f);
    // font_small = io.Fonts->AddFontFromFileTTF("./fonts/font.ttf", 10.0f);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    bool running = true;

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                if(status == true)
                {
                    string command="logout " + current_user.username; 
                    send_to_server(command.c_str());
                }
                running = false;
            }
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        NewFrame();

        switch (current_page)
        {
        case Page::LOGIN:
            render_LOGIN();
            break;
        case Page::REGISTER:
            render_REGISTER();
            break;
        case Page::HOME:
            render_HOME();
            break;
        case Page::PROFILE:
            render_PROFILE();
            break;
        case Page::FRIENDS:
            render_FRIENDS();
            break;
        case Page::POSTS:
            render_POSTS();
            break;
        case Page::CLOSE:
            render_CLOSE();
            break;
        case Page::FRIEND_POSTS:
            render_FRIEND_POSTS();
            break;
        case Page::CONSOLE:
            render_CONSOLE();
            break;
        case Page::MESSAGES:
            render_MESSAGES();
            break;
        }

        render_error();

        Render();
        glViewport(0, 0, 800, 600);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());

        SDL_GL_SwapWindow(window);
    }
    disconnect_from_server();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
