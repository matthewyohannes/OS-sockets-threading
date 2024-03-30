#include <unistd.h>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <fstream>
#include <pthread.h>
#include <iomanip>
#include <utility>
#include <cmath>
#include <memory>


// Credit to Dr. Rincon for the sockets code
// References https://gitgud.io/MadDawg/cosc3360_spring2024_guides/-/blob/master/assignment1guide.md?ref_type=heads//


std::vector<std::string> get_inputs() { // function to receive inputs
    std::vector<std::string> inputs;
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input.empty()) // Check for empty line
            break;
        inputs.push_back(input);
    }
    return inputs;
}

struct storeArg { // struct that holds data for each instance of an input line
    std::string input;
    std::string *output;
    std::size_t iteration;
    char *address;
    int port;

    storeArg() = default;

    storeArg(const std::string &in, std::string *out, std::size_t iter, char *serverIp, int portNum) { // constructor for struct object
        input = in;
        output = out;
        iteration = iter;
        address = serverIp;
        port = portNum;
    }
};

std::string create_output(const std::string &input, const int iteration) {
    std::stringstream output_sstream;
    std::stringstream input_sstream(input);

    std::string line;
    while (std::getline(input_sstream, line)) {
        // modify CPU line to include the iteration number
        if (line.find("CPU") != std::string::npos) {
            size_t pos = line.find("CPU");
            if (pos != std::string::npos) {
                // if there is no iteration number, add it after "CPU" with a space
                if (line.find(":") != std::string::npos) {
                    line.insert(pos + 3, std::to_string(iteration));
                } else {
                    // remove the iteration number and add the correct one
                    size_t end_pos = line.find_first_not_of("0123456789", pos + 3);
                    if (end_pos != std::string::npos && line[end_pos] == '0' && end_pos - pos == 4) {
                        line.erase(end_pos);
                    }
                    line.replace(pos, end_pos - pos, "CPU " + std::to_string(iteration));
                }
            }
        }
        if (line.find("Rate Monotonic Algorithm execution for CPU") != std::string::npos) {
            size_t pos = line.find("CPU");
            if (pos != std::string::npos) {
                size_t end_pos = line.find_first_not_of("0123456789", pos + 3);
                if (end_pos != std::string::npos) {
                    line.replace(pos, end_pos - pos, "CPU" + std::to_string(iteration));
                }
            }
        }
        output_sstream << line << "\n";
    }

    return output_sstream.str();
}





void *thread_worker(void *arguments) {
    storeArg *args = static_cast<storeArg *>(arguments);
    auto &[input, output, iteration, address, portno] = *args; // destruct struct variables

    int sockfd, n;
    std::string buffer;
    struct sockaddr_in serv_addr;
    struct hostent *server; // set up socket

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "ERROR opening socket" << std::endl;
        exit(0);
    }
    server = gethostbyname(address);
    if (server == NULL) {
        std::cerr << "ERROR, no such host" << std::endl;
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "ERROR connecting" << std::endl;
        exit(0);
    }
    
    buffer = input; // assign input to buffer to get sent to server
    int msgSize = buffer.size(); // set up buffer size
    n = write(sockfd, &msgSize, sizeof(int));
    if (n < 0) {
        std::cerr << "ERROR writing to socket" << std::endl;
        exit(0);
    }
    n = write(sockfd, buffer.c_str(), msgSize);
    if (n < 0) {
        std::cerr << "ERROR writing to socket" << std::endl;
        exit(0);
    }
    n = read(sockfd, &msgSize, sizeof(int));
    if (n < 0) {
        std::cerr << "ERROR reading from socket" << std::endl;
        exit(0);
    }
    char *tempBuffer = new char[msgSize + 1];
    bzero(tempBuffer, msgSize + 1);
    n = read(sockfd, tempBuffer, msgSize);
    if (n < 0) {
        std::cerr << "ERROR reading from socket" << std::endl;
        exit(0);
    }
    buffer = tempBuffer;
    delete[] tempBuffer; // delete tempbuffer allocation
    close(sockfd); // close socket

    *output = create_output(buffer, iteration); // add formatted output to output variable
    return nullptr;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "usage " << argv[0] << " hostname port" << std::endl;
        exit(0);
    }

    std::string address = argv[1]; // initialize server address
    int portno = atoi(argv[2]); // intialize port number

    const std::vector<std::string> inputs = get_inputs();
    std::vector<std::string> outputs(inputs.size());
    std::vector<pthread_t> pthreads(inputs.size());
    std::vector<storeArg> arg_objects; // vector of object instances for each process

    for (std::size_t i = 0; i < inputs.size(); ++i) {
        arg_objects.emplace_back(inputs[i], &outputs[i], i + 1, argv[1], portno); // add input data for each process to the variables
    }

    for (std::size_t i = 0; i < inputs.size(); ++i) {
        pthread_create(&pthreads[i], nullptr, thread_worker, &arg_objects[i]);
    }

    for (std::size_t i = 0; i < inputs.size(); ++i) {
        pthread_join(pthreads[i], nullptr);
    }

    for (const auto &output : outputs) {
        std::cout << output << std::endl; // output final result processes
    }

    return 0;
}
