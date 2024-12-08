#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sys/types.h>

using namespace std;

enum State { R, S };

static pid_t other_pid = 0;
static State my_state;
static bool is_process_a = false;

static const char* pidfile = "pidfile.txt";

void signal_handler(int signo, siginfo_t *info, void *context) {

    pid_t sender_pid = info->si_pid;

    if (my_state == S) {
        cout << "proc[" << getpid() << "]: signal received from " << sender_pid
             << " (ping/pong received)" << endl;

        my_state = R;

        cout << "proc[" << getpid() << "]: sending signal back" << endl;
        if (kill(other_pid, SIGUSR1) == -1) {
            cerr << "Error sending signal: " << strerror(errno) << endl;
        }

        my_state = S;
    } else {
        cout << "proc[" << getpid() << "]: received signal in R state (unexpected)" << endl;
    }
}

int main(int argc, char* argv[]) {
    pid_t my_pid = getpid();

    if (argc == 1) {
        is_process_a = true;
        my_state = R;

        {
            ofstream ofs(pidfile);
            if (!ofs) {
                cerr << "Cannot open pidfile for writing\n";
                return 1;
            }
            ofs << my_pid << "\n";
        }

        cout << "proc[" << my_pid << "]: I am A, wrote my PID to pidfile, waiting for B's PID..." << endl;

        pid_t b_pid = 0;
        for (;;) {
            ifstream ifs(pidfile);
            if (ifs) {
                pid_t a_pid_check;
                ifs >> a_pid_check;
                ifs >> b_pid;

                if (b_pid > 0 && a_pid_check == my_pid) {
                    break;
                }
            }
            usleep(100000);
        }

        other_pid = b_pid;
        cout << "proc[" << my_pid << "]: Got B's PID: " << other_pid << endl;

    } else {
        if (argc < 2) {
            cerr << "Usage: " << argv[0] << " <PID_of_A>\n";
            return 1;
        }
        pid_t a_pid = atoi(argv[1]);
        if (a_pid <= 0) {
            cerr << "Invalid PID of A\n";
            return 1;
        }
        other_pid = a_pid;
        my_state = S;

        {
            ifstream ifs(pidfile);
            pid_t a_pid_check;
            if (!ifs || !(ifs >> a_pid_check) || a_pid_check != a_pid) {
                cerr << "Cannot verify A PID in pidfile\n";
                return 1;
            }
        }

        {
            ofstream ofs(pidfile);
            ofs << other_pid << "\n" << my_pid << "\n";
        }
        cout << "proc[" << my_pid << "]: I am B, known A's PID = " << other_pid << endl;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        cerr << "Error sigaction: " << strerror(errno) << endl;
        return 1;
    }

    if (is_process_a && my_state == R && other_pid != 0) {
        cout << "proc[" << getpid() << "]: initial send signal to B" << endl;
        if (kill(other_pid, SIGUSR1) == -1) {
            cerr << "Error sending signal: " << strerror(errno) << endl;
        }
        my_state = S;
    }

    for (;;) {
        pause();
    }

    return 0;
}
