#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <string>
#include <cstring>
#include <sstream>
#include <fstream>
#include <pthread.h>
#include <iomanip>
#include <unistd.h>
#include <utility>
#include <cmath>
#include <memory>
#include <tuple>
#include <queue>

using namespace std;

// input function from user, used to store input in a vector//
std::vector<std::string> get_inputs()
{
    std::vector<std::string> inputs;
    std::string input;
    while (std::getline(std::cin, input))
    {
        if (input.empty()) // Check for empty line
            break;
        inputs.push_back(input);
    }
    return inputs;
}

// struct used to hold args for pthread //
struct storeArg
{

    std::string input;
    std::string *output;
    std::size_t iteration;

    storeArg() = default;

    storeArg(const std::string &in, std::string *out, std::size_t iter)
    {
        input = in;
        output = out;
        iteration = iter;
    }
};

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
        return lhs.period > rhs.period; // Compare tasks based on their periods//
    }
};

std::string generate_scheduling_diagram(const unsigned hyperperiod, std::vector<Task> &tasks)
{
    std::vector<TaskInterval> intervals;
    std::vector<Task> runningTasks = tasks; // Copy of tasks to manipulate running states
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

        // Schedule tasks based on priority and remaining WCET
        for (Task &task : runningTasks)
        {
            // if WCET value is > zero
            if (task.wcet > 0)
            {
                TaskInterval interval(task.name, currentTime, currentTime + 1, false);
                intervals.push_back(interval);
                task.wcet--; // decrement the task's WCET value
                taskScheduled = true;
                break; // A task is scheduled for this tick
            }
        }

        // Handle idle times
        if (!taskScheduled)
        {
            if (!intervals.empty() && intervals.back().name == "Idle" && intervals.back().endTime == currentTime)
            {
                intervals.back().endTime++; // Extend the last idle interval
            }
            else
            {
                intervals.emplace_back("Idle", currentTime, currentTime + 1, false); // New idle interval
            }
        }

        currentTime++;
    }

    // Construct the given scheduling diagram
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
            lastInterval.endTime = interval.endTime; // Extend the current interval
        }
    }
    if (lastInterval.name != "")
    {
        schedulingDiagram << lastInterval.name << "(" << lastInterval.endTime - lastInterval.startTime << ")";
    }

    return schedulingDiagram.str();
}

std::string parse(const std::string &input, std::size_t iteration)
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

    // Extract tasks from input string
    while (inputString >> taskName >> taskWcet >> taskPeriod)
    {
        tasks.emplace_back(std::string(1, taskName), taskWcet, taskPeriod);
    }

    // Calculate threshold, utilization, and hyperperiod here
    threshold = thresholdCalc(tasks.size());
    utilization = utilizationCalc(tasks);
    hyperperiod = hyperperiodCalc(tasks);

    // Output strings
    if (hyperperiod <= 0)
        return "";

    if (iteration > 1)
    {
        entropyValues << "\n\n";
    }
    entropyValues << "CPU " << iteration << "\n"
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
                  << "\nRate Monotonic Algorithm execution for CPU" << iteration << ":";

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
        // Sort tasks based on their periods to ensure they are in priority order for RMS
        std::sort(tasks.begin(), tasks.end(), [](const Task &a, const Task &b)
                  { return a.period < b.period; });

        // Inserting the scheduling diagram into our output
        entropyValues << "\nScheduling Diagram for CPU " << iteration << ": ";
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

// fuction to convert void pointer to storeArg* aswell as deref the pointer to access input string,output and iteration strings//
void *threads(void *arguments)
{
    storeArg *args = static_cast<storeArg *>(arguments);
    auto &[input, output, iteration] = *args;
    // call parse to proccess the strings and give output//

    *output = parse(input, iteration);
    return nullptr;
}

// A 2 10 B 4 15 C 3 30
// X 10 25 Y 5 10 Z 1 20
// A 1 5 B 5 20 C 1 10 D 1 5
int main()
{
    const std::vector<std::string> inputs = get_inputs();
    std::vector<std::string> outputs(inputs.size());
    std::vector<pthread_t> pthreads(inputs.size());
    std::vector<storeArg> arg_objects;
    // store args //
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        // std::cout << inputs[i] << " " << &outputs[i] << " " << i + 1 << std::endl;
        arg_objects.emplace_back(inputs[i], &outputs[i], i + 1);
    }
    // pass args to thread func and create thread//
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        // pass a pointer to pthread object, pointer object that configs att of thread, void* func that expects a void* arg, void* obj that will be passed into void* func as an arg//
        // std::cout << &arg_objects[i].input << std::endl;
        pthread_create(&pthreads[i], nullptr, threads, &arg_objects[i]);
    }
    // join loop that waits on thread create loop//
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        // pass a pthreads obj and a pointer to obj that will store return value of the void* func//
        pthread_join(pthreads[i], nullptr);
    }
    // print output container//
    for (const auto &output : outputs)
    {
        std::cout << output << "\n";
    }
}
// references https://gitgud.io/MadDawg/cosc3360_spring2024_guides/-/blob/master/assignment1guide.md?ref_type=heads//