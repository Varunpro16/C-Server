#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <map>
#include <vector>
#include <fstream>

using namespace std;

// User types
enum UserType { STUDENT, TEACHER };

// User credentials and types
map<string, pair<string, UserType>> userCredentials;

// Student marks
map<string, vector<int>> studentMarks;

// Load user credentials from file
void loadUserCredentials() {
    ifstream infile("users.txt");
    string line;

    while (getline(infile, line)) {
        stringstream ss(line);
        string username, password, type;
        ss >> username >> password >> type;
        UserType userType = (type == "STUDENT") ? STUDENT : TEACHER;
        userCredentials[username] = {password, userType};
    }
    infile.close();
}

// Save user credentials to file
void saveUserCredentials() {
    ofstream outfile("users.txt");
    for (const auto& entry : userCredentials) {
        outfile << entry.first << " " << entry.second.first << " "
                << (entry.second.second == STUDENT ? "STUDENT" : "TEACHER") << "\n";
    }
    outfile.close();
}

// Load student marks from file
map<string, vector<int>> loadStudentMarks() {
    ifstream infile("marks.txt");
    string line;
    map<string, vector<int>> studentMarks;

    while (getline(infile, line)) {
        stringstream ss(line);
        string username;
        vector<int> marksList;

        ss >> username;
        int mark;
        while (ss >> mark) {
            marksList.push_back(mark);
        }
        studentMarks[username] = marksList;
    }
    infile.close();
    return studentMarks;
}

// Save student marks to file
void saveStudentMarks() {
    ofstream outfile("marks.txt");
    for (const auto& entry : studentMarks) {
        outfile << entry.first;
        for (int mark : entry.second) {
            outfile << " " << mark;
        }
        outfile << "\n";
    }
    outfile.close();
}

vector<string> SplitPlus(const string& s) {
    vector<string> ans;
    string t;
    for (char c : s) {
        if (c == '+') {
            ans.push_back(t);
            t.clear();
        } else {
            t += c;
        }
    }
    ans.push_back(t);
    return ans;
}

// Handle client request
void handleClient(int client_fd) {
    char buffer[1024] = {0};
    ssize_t bytesRead = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
        close(client_fd);
        return;
    }

    buffer[bytesRead] = '\0';
    string request(buffer);
 

    if (request.size() >= 4 && request.substr(0, 4) == "POST") {
        size_t pos = request.find("\r\n\r\n");
        if (pos == string::npos) {
            // Bad request
            string response = "HTTP/1.1 400 Bad Request\r\n"
                              "Content-Type: text/html\r\n\r\n"
                              "<html><body><h1>Bad Request</h1>"
                              "<p>Invalid request format.</p></body></html>";
            send(client_fd, response.c_str(), response.length(), 0);
            close(client_fd);
            return;
        }
        
        string postData = request.substr(pos + 4);

        size_t userPos = postData.find("username=");
        size_t passPos = postData.find("password=");
        size_t updatePos = postData.find("update=");

        if (updatePos != string::npos) {
            // Handle update request
            string updateData = postData.substr(updatePos + 7);
            vector<string> infos = SplitPlus(updateData);
            if (infos.size() != 3) {
                string response = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: text/html\r\n\r\n"
                                  "<html><body><h1>Bad Request</h1>"
                                  "<p>Invalid update data format.</p></body></html>";
                send(client_fd, response.c_str(), response.length(), 0);
                close(client_fd);
                return;
            }

            string username = infos[0];
            string subject = infos[1];
            int newMarks = stoi(infos[2]);

            studentMarks = loadStudentMarks(); // Reload marks

            int index = -1;
            if (subject == "Maths") index = 0;
            if (subject == "Science") index = 1;
            if (subject == "English") index = 2;
            if (subject == "Social") index = 3;
            if (subject == "Computer") index = 4;

            if (index != -1 && studentMarks.find(username) != studentMarks.end()) {
                studentMarks[username][index] = newMarks;
                // first update the local data structure
                // then update the changes to the file 
                saveStudentMarks(); // Save updated marks

                string response = "HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/html\r\n\r\n"
                  "<html><body><h1>Update Successful</h1>"
                  "<p>The marks have been updated successfully.</p>"
                  "<a href='/'>Go back</a></body></html>";

                send(client_fd, response.c_str(), response.length(), 0);
            } else {
                // Unauthorized
                string response = "HTTP/1.1 401 Unauthorized\r\n"
                                  "Content-Type: text/html\r\n\r\n"
                                  "<html><body><h1>Unauthorized</h1>"
                                  "<p>Invalid username or insufficient permissions.</p></body></html>";
                send(client_fd, response.c_str(), response.length(), 0);
            }
        } else if (userPos != string::npos && passPos != string::npos) {
            string username = postData.substr(userPos + 9, postData.find('&', userPos) - userPos - 9);
            string password = postData.substr(passPos + 9);

            if (userCredentials.find(username) != userCredentials.end() &&
                userCredentials[username].first == password) {
                UserType userType = userCredentials[username].second;

                if (userType == STUDENT) {
                    // Student access: Show their own details
                    studentMarks = loadStudentMarks(); // Reload marks
                    // loading the local data structure
                    const auto& marksList = studentMarks[username];

                    string marksString;
                    int sumMarks = 0, i = 0, index = -1, maxiMark = -1;
                    // preparing client page string for output since we pass the html as string
                    // like
                    // Maths 89
                    // Science 90
                    for (int marks : marksList) {
                        string subject;
                        switch (i) {
                            case 0: subject = "Maths"; break;
                            case 1: subject = "Science"; break;
                            case 2: subject = "English"; break;
                            case 3: subject = "Social"; break;
                            case 4: subject = "Computer"; break;
                        }
                        marksString += "<p>" + subject + ": " + to_string(marks) + "</p>";
                        sumMarks += marks;
                        if (marks > maxiMark) {
                            maxiMark = marks;
                            index = i;
                        }
                        i++;
                    }
                    string subject = (index == 0 ? "Maths" : 
                                      (index == 1 ? "Science" : 
                                       (index == 2 ? "English" :
                                        (index == 3 ? "Social" : "Computer"))));
                    //  sending page as string 
                    string response = "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html\r\n\r\n"
                                      "<html><body><h1>Details for " + username + "</h1>"
                                      + marksString +
                                      "<p>Aggregate Percentage: " + to_string((sumMarks * 100) / 500) + "</p>"
                                      "<p>Maximum Marks obtained is " + to_string(maxiMark) + " in " + subject + "</p>"
                                      "</body></html>";
                    send(client_fd, response.c_str(), response.length(), 0);
                } else if (userType == TEACHER) {
                    // Teacher access: Show all student details and allow updates
                    studentMarks = loadStudentMarks(); // Reload marks
                    stringstream allMarks;
                    allMarks << "<html><body><h1>All Student Details</h1>";
                    //iterating over all the students in local data structure
                    for (const auto& entry : studentMarks) {
                        allMarks << "<h2>" << entry.first << "</h2>";
                        const auto& marksList = entry.second;

                        int sumMarks = 0, i = 0, index = -1, maxiMark = -1;
                        string marksString;
                        // iterating over marks of individual students
                        for (int marks : marksList) {
                            string subject;
                            switch (i) {
                                case 0: subject = "Maths"; break;
                                case 1: subject = "Science"; break;
                                case 2: subject = "English"; break;
                                case 3: subject = "Social"; break;
                                case 4: subject = "Computer"; break;
                            }
                            marksString += "<p>" + subject + ": " + to_string(marks) + "</p>";
                            sumMarks += marks;
                            if (marks > maxiMark) {
                                maxiMark = marks;
                                index = i;
                            }
                            i++;
                        }
                        string subject = (index == 0 ? "Maths" : 
                                          (index == 1 ? "Science" : 
                                           (index == 2 ? "English" :
                                            (index == 3 ? "Social" : "Computer"))));

                        allMarks << marksString
                                 << "<p>Aggregate Percentage: " + to_string((sumMarks * 100) / 500) + "</p>"
                                 << "<p>Maximum Marks obtained is " + to_string(maxiMark) + " in " + subject + "</p>";
                    }
                    // sending the html page with all the students detaila along with the edit button
                    // to edit the marks
                    // adding the request send functions using javascipt to reach the c++ server
                    allMarks << "<button onClick='handleEdit()'>Click to Edit Marks</button>\n"
                             << "<script>\n"
                             << "async function sendData() {\n"
                             << "    const url = 'http://localhost:5555';\n"
                             << "    const data = prompt('Enter update data (e.g., username+Maths+45):');\n"
                             << "    const [username, subject, marks] = data.split('+');\n"
                             << "    const payload = new URLSearchParams({\n"
                             << "        update: `${username}+${subject}+${marks}`\n"
                             << "    });\n"
                             << "    try {\n"
                             << "        const response = await fetch(url, {\n"
                             << "            method: 'POST',\n"
                             << "            headers: {\n"
                             << "                'Content-Type': 'application/x-www-form-urlencoded',\n"
                             << "            },\n"
                             << "            body: payload\n"
                             << "        });\n"
                             << "        const text = await response.text();\n"
                             << "        console.log('Response:', text);\n"
                             << "    } catch (error) {\n"
                             << "        console.error('Error:', error);\n"
                             << "    }\n"
                             << "}\n"
                             << "const handleEdit = () => {\n"
                             << "    sendData();\n"
                             << "};\n"
                             << "</script>\n"
                             << "</body></html>";

                    string response = "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html\r\n\r\n" + allMarks.str();
                    send(client_fd, response.c_str(), response.length(), 0);
                }
            } else {
                // Authentication failed
                string response = "HTTP/1.1 401 Unauthorized\r\n"
                                  "Content-Type: text/html\r\n\r\n"
                                  "<html><body><h1>Unauthorized</h1>"
                                  "<p>Invalid username or password.</p></body></html>";
                send(client_fd, response.c_str(), response.length(), 0);
            }
        } else {
            // Bad request
            string response = "HTTP/1.1 400 Bad Request\r\n"
                              "Content-Type: text/html\r\n\r\n"
                              "<html><body><h1>Bad Request</h1>"
                              "<p>Missing username or password.</p></body></html>";
            send(client_fd, response.c_str(), response.length(), 0);
        }
    } else {
        // Method not allowed
        string response = "HTTP/1.1 405 Method Not Allowed\r\n"
                          "Content-Type: text/html\r\n\r\n"
                          "<html><body><h1>Method Not Allowed</h1>"
                          "<p>Only POST requests are supported.</p></body></html>";
        send(client_fd, response.c_str(), response.length(), 0);
    }

    close(client_fd);
}

int main() {
    loadUserCredentials();
    studentMarks = loadStudentMarks(); // Load student marks once at the start

    int server_fd, client_fd;
    sockaddr_in server_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    // setting up the server object(socket object)
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(5555);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    cout << "Server is listening on port 5555...\n";

    while (true) {
        // runs till the client connects to the server
        if ((client_fd = accept(server_fd, NULL, NULL)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        handleClient(client_fd);
    }

    close(server_fd);
    return 0;
}

