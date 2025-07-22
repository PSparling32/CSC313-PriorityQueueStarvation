#include <iostream> //in out
#include <queue> //priority queue
#include <thread> //threading
#include <vector> //for vector to store things
#include <functional> //lets functions be treated as objects
#include <atomic> //atomic is used to ensure that the running state is thread-safe
#include <chrono> //for time management
#include <mutex> //mutex is used to protect access to the priority queue
#include <condition_variable> //condition variable is a variable that blocks the thread until a certain condition is met

struct TaskItem { // Structure to hold task information
    int priority; //high value higher priority
	std::function<void()> task; //std::function is use to store the task function, letting functions be passed as tasks
    bool operator<(const TaskItem& other) const { //makes it a callable object
		return priority < other.priority; //return true if this task has lower priority than the other
    }
};

// Global flag for exit task
std::atomic<bool> exit_executed{false};

// Work task function
void work_task() {
    std::cout << "Work executed by thread " << std::this_thread::get_id() << std::endl;
}

// Exit task function
void exit_task() {
    std::cout << "Exit executed by thread " << std::this_thread::get_id() << " (should be starved)" << std::endl;
    exit_executed = true;
}

int main() {
	std::priority_queue<TaskItem> pq; // Priority queue to hold tasks
	std::mutex mtx; // Mutex to protect access to the priority queue
	std::condition_variable cv; //condition variable to manage task availability
	std::atomic<bool> running{ true }; //control variable to manage the running state of the threads

    // Producer thread: keeps adding high-priority tasks
    std::thread producer([&] { //this thread constantly pushes tasks
        while (running) { //run untill running is false
			{// Lock the mutex to protect access to the priority queue
                std::lock_guard<std::mutex> lock(mtx);
				pq.push(TaskItem{ 10, work_task});
			}//release lock so other threads can access the queue
			cv.notify_one();// Notify one waiting thread that a task is available
			std::this_thread::sleep_for(std::chrono::milliseconds(60));//sleep for 60 milliseconds
        }
    });

    // Add low-priority exit task once
    // this will be starved by the high-priority tasks
    {
        std::lock_guard<std::mutex> lock(mtx);
        pq.push(TaskItem{1, exit_task});
    }
	// Notify all threads that a task is available
    cv.notify_all();

    // stores worker threads
    std::vector<std::thread> workers;

    //control varibale for thread count
    int num_workers = 3;

	// Create worker threads
    for (int i = 0; i < num_workers; ++i) {
		workers.emplace_back([&] { //emplace_back adds a new thread to the vector. What this is doing is making a lamda function that will be run by the thread
			                       // lamda function is a function that 
            while (running) {
                TaskItem item;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [&] { return !pq.empty() || !running; });
                    if (!running && pq.empty()) break;
                    item = pq.top();
                    pq.pop();
                }
                item.task();
                // Aging: increment priority of all waiting tasks
                std::vector<TaskItem> tempTasks;
                while (!pq.empty()) {
                    TaskItem t = pq.top();
                    pq.pop();
                    t.priority += 1; // Increase priority (aging)
                    tempTasks.push_back(t);
                }
                for (auto& t : tempTasks) pq.push(t);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }

    // Run for a fixed time
    std::this_thread::sleep_for(std::chrono::seconds(3));
    running = false;
    cv.notify_all();

    producer.join();
    for (auto& t : workers) t.join();

    if (!exit_executed)
        std::cout << "Exit was starved and never executed." << std::endl;
    else
        std::cout << "Exit was executed." << std::endl;

    std::cout << "Simulation complete." << std::endl;
    return 0;
}