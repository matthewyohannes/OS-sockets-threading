#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/wait.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <pthread.h>
#include <iomanip>
#include <utility>
#include <cmath>
#include <memory>
#include <queue>

// Credit to Dr. Rincon for the sockets code
// References https://gitgud.io/MadDawg/cosc3360_spring2024_guides/-/blob/master/assignment1guide.md?ref_type=heads


void fireman(int)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

struct Task
{
    std::string name;
    int wcet;
    int initialWcet;
    int period;
    // constructor for struct used to set WCET to initial WCET and sets the task name and period//
    Task(const std::string &taskName, int wcetValue, int periodValue)
    {
        name = taskName;
        wcet = wcetValue;
        initialWcet = wcetValue;
        period = periodValue;
    }
    // function used to reset WCET//
    void resetWcet()
    {
        wcet = initialWcet;
    }
    // used to compare tasks by overloading < op//
    bool operator<(const Task &other) const
    {
        return (period < other.period || (name < other.name && period == other.period));
    }
};

// struct that stores taskname start time end time and stopped. Used to determine idle time//
struct TaskInterval
{
    std::string name;
    int startTime;
    int endTime;
    bool stopped;
    // constructor for struct, used to set isStopped to false//
    TaskInterval(const std::string &taskName, int start, int end, bool isStopped = false)
    {
        name = taskName, startTime = start, endTime = end, stopped = isStopped;
    }
};
// calc the threshold for the rmsa by using utilization threshold formula//
double thresholdCalc(std::size_t numTask)
{
    return numTask * (std::pow(2, 1.0 / numTask) - 1);
}

int gcd(int a, int b)
{
    while (b != 0)
    {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int lcm(int a, int b)
{
    return (a * b) / gcd(a, b);
}
// calc hyperperiod by taking a reference to a vector of task as the parameter//
int hyperperiodCalc(const std::vector<Task> &tasks)
{
    // init var to the period of first task in vector//
    int hyperperiod = tasks[0].period;
    // it over the rest of the tasks in vector//
    for (std::size_t i = 1; i < tasks.size(); ++i)
    {
        // update hyperperiod by calc lcm of current HP and the period of the task at i//
        hyperperiod = lcm(hyperperiod, tasks[i].period);
    }
    return hyperperiod;
}
// calc utilization of tasks by itr through each task//
double utilizationCalc(const std::vector<Task> &tasks)
{
    double total_utilization = 0.0;
    for (const auto &task : tasks)
    {
        // calc uti by didviding wcet by the period //
        total_utilization += static_cast<double>(task.wcet) / task.period;
    }
    return total_utilization;
}

struct TaskCompare
{
    bool operator()(const Task &lhs, const Task &rhs) const
    {
        return lhs.period > rhs.period; // compare tasks based on their periods//
    }
};

std::string generate_scheduling_diagram(const unsigned hyperperiod, std::vector<Task> &tasks)
{
    std::vector<TaskInterval> intervals;
    std::vector<Task> runningTasks = tasks; // copy of tasks to manipulate running states
    unsigned int currentTime = 0;
    std::stringstream schedulingDiagram;

    while (currentTime < hyperperiod)
    {
        bool taskScheduled = false;

        for (Task &task : runningTasks)
        {
            if (currentTime != 0 && currentTime % task.period == 0)
            {
                task.resetWcet(); // reset that task's WCET value to its initial values

                // go through our list of task intervals and mark all of them as stopped
                for (TaskInterval &interval : intervals)
                {
                    if (interval.name == task.name && !interval.stopped && interval.endTime <= currentTime)
                    {
                        interval.stopped = true;
                    }
                }
            }
        }

        // schedule tasks based on priority and remaining WCET
        for (Task &task : runningTasks)
        {
            // if WCET value is > zero
            if (task.wcet > 0)
            {
                TaskInterval interval(task.name, currentTime, currentTime + 1, false);
                intervals.push_back(interval);
                task.wcet--; // decrement the task's WCET value
                taskScheduled = true;
                break; // a task is scheduled for this tick
            }
        }

        // Handle idle times
        if (!taskScheduled)
        {
            if (!intervals.empty() && intervals.back().name == "Idle" && intervals.back().endTime == currentTime)
            {
                intervals.back().endTime++; // extend the last idle interval
            }
            else
            {
                intervals.emplace_back("Idle", currentTime, currentTime + 1, false); // new idle interval
            }
        }

        currentTime++;
    }

    // construct the given scheduling diagram
    TaskInterval lastInterval = intervals.empty() ? TaskInterval{"", 0, 0, false} : intervals[0];
    for (const TaskInterval &interval : intervals)
    {
        if (interval.name != lastInterval.name)
        {
            if (lastInterval.name != "")
            {
                schedulingDiagram << lastInterval.name << "(" << lastInterval.endTime - lastInterval.startTime << "), "; // calculate its runtime by subtracting the interval's start from its end
            }
            lastInterval = interval;
        }
        else
        {
            lastInterval.endTime = interval.endTime; // extend the current interval
        }
    }
    if (lastInterval.name != "")
    {
        schedulingDiagram << lastInterval.name << "(" << lastInterval.endTime - lastInterval.startTime << ")";
    }

    return schedulingDiagram.str();
}

std::string parse(const std::string &input)
{
    std::stringstream inputString(input);
    std::stringstream entropyValues;
    std::vector<Task> tasks;
    char taskName;
    unsigned taskWcet;
    unsigned taskPeriod;
    double threshold;
    double utilization;
    int hyperperiod;

    auto comp = [](const Task &l, const Task &r)
    { return r < l; };
    std::priority_queue<Task, std::vector<Task>, decltype(comp)> task_queue(tasks.begin(), tasks.end(), comp);

    // extract tasks from input string
    while (inputString >> taskName >> taskWcet >> taskPeriod)
    {
        tasks.emplace_back(std::string(1, taskName), taskWcet, taskPeriod);
    }

    // calculate threshold, utilization, and hyperperiod here
    threshold = thresholdCalc(tasks.size());
    utilization = utilizationCalc(tasks);
    hyperperiod = hyperperiodCalc(tasks);

    // output strings
    if (hyperperiod <= 0)
        return "";


    entropyValues << "CPU " << "\n"
                  << "Task scheduling information: ";
    for (std::size_t i = 0; i < tasks.size(); i++)
    {
        entropyValues << tasks[i].name << " (WCET: " << tasks[i].wcet << ", Period: " << tasks[i].period << ")";
        if (i < tasks.size() - 1)
        {
            entropyValues << ", ";
        }
    }

    entropyValues << "\nTask set utilization: " << std::setprecision(2) << std::fixed << utilization
                  << "\nHyperperiod: " << hyperperiod
                  << "\nRate Monotonic Algorithm execution for CPU" << ":";

    if (utilization > 1)
    {
        entropyValues << "\nThe task set is not schedulable";
        return entropyValues.str();
    }
    else if (utilization > threshold && utilization <= 1)
    {
        entropyValues << "\nTask set schedulability is unknown";
        return entropyValues.str();
    }
    else
    {
        // sort tasks based on their periods to ensure they are in priority order for RMS
        std::sort(tasks.begin(), tasks.end(), [](const Task &a, const Task &b)
                  { return a.period < b.period; });

        // inserting the scheduling diagram into our output
        entropyValues << "\nScheduling Diagram for CPU "<< ": ";
        entropyValues << generate_scheduling_diagram(hyperperiod, tasks);
    }
    tasks.clear();
    while (!task_queue.empty())
    {
        tasks.push_back(task_queue.top());
        task_queue.pop();
    }

    return entropyValues.str();
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;

    signal(SIGCHLD, fireman);

    // check the commandline arguments
    if (argc != 2)
    {
        std::cerr << "Port not provided" << std::endl;
        exit(0);
    }

    // create the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        std::cerr << "Error opening socket" << std::endl;
        exit(0);
    }

    // populate the sockaddr_in structure
    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // bind the socket with the sockaddr_in structure
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "Error binding" << std::endl;
        exit(0);
    }

    // set the max number of concurrent connections
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    // hyper, utilization, schedule
    // double int string

    while (true)
    {

        // accept a new connection
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen);

        if (fork() == 0) // beginning of fork
        {
            if (newsockfd < 0)
            {
                std::cerr << "Error accepting new connections" << std::endl; // error handling
                exit(0);
            }
            int n, msgSize = 0;
            n = read(newsockfd, &msgSize, sizeof(int));
            if (n < 0)
            {
                std::cerr << "Error reading from socket" << std::endl;
                exit(0);
            }
            char *tempBuffer = new char[msgSize + 1];
            bzero(tempBuffer, msgSize + 1);
            n = read(newsockfd, tempBuffer, msgSize + 1);
            if (n < 0)
            {
                std::cerr << "Error reading from socket" << std::endl;
                exit(0);
            }
            std::string buffer = tempBuffer;
            delete[] tempBuffer;
            ///
            //std::cout << "Message from client: " << buffer << ", Message size: " << msgSize << std::endl; // message receiving from client input
            buffer = parse(buffer);                                                                        // prepping message to send to client (parse)
            msgSize = buffer.size();
            n = write(newsockfd, &msgSize, sizeof(int));
            if (n < 0)
            {
                std::cerr << "Error writing to socket" << std::endl;
                exit(0);
            }
            n = write(newsockfd, buffer.c_str(), msgSize);
            if (n < 0)
            {
                std::cerr << "Error writing to socket" << std::endl;
                exit(0);
            }
            close(newsockfd); // close socket
        }
    }

    close(sockfd); // close original socket
    return 0;
}