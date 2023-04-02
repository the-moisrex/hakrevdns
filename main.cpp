#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netdb.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;

struct Options {
  int threads = 8;
  string resolver_ip;
  string protocol = "udp";
  uint16_t port = 53;
  bool domain = false;
};

Options opts;

void parse_options(int argc, char *argv[]) {
  int opt;

  while ((opt = getopt(argc, argv, "t:r:P:p:d")) != -1) {
    switch (opt) {
    case 't':
      opts.threads = atoi(optarg);
      break;
    case 'r':
      opts.resolver_ip = optarg;
      break;
    case 'P':
      opts.protocol = optarg;
      break;
    case 'p':
      opts.port = atoi(optarg);
      break;
    case 'd':
      opts.domain = true;
      break;
    default:
      cerr << "Usage: " << argv[0]
           << " [-t <threads>] [-r <resolver IP>] [-P <protocol>] [-p <port>] "
              "[-d]"
           << endl;
      exit(1);
    }
  }
}

vector<string> read_input() {
  vector<string> ips;
  string line;

  while (getline(cin, line)) {
    ips.push_back(line);
  }

  return ips;
}

void do_work(const string &ip) {
  static mutex cout_mutex;
  static mutex resolver_mutex;
  static addrinfo *resolver = nullptr;

  if (!resolver) {
    lock_guard<mutex> lock(resolver_mutex);

    if (!resolver) {
      addrinfo hints, *res;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = (opts.protocol == "tcp") ? SOCK_STREAM : SOCK_DGRAM;

      int err = getaddrinfo(opts.resolver_ip.c_str(),
                            to_string(opts.port).c_str(), &hints, &res);

      if (err) {
        cerr << "[ERROR] Failed to resolve DNS server address: "
             << gai_strerror(err) << endl;
        exit(1);
      }

      resolver = res;
    }
  }

  char host[NI_MAXHOST];

  int err = getnameinfo(resolver->ai_addr, resolver->ai_addrlen, host,
                        sizeof(host), nullptr, 0, 0);

  if (err) {
    cerr << "[ERROR] Failed to resolve DNS server address: "
         << gai_strerror(err) << endl;
    exit(1);
  }

  addrinfo *result;

  err = getaddrinfo(ip.c_str(), nullptr, nullptr, &result);

  if (err) {
    return;
  }

  vector<string> domains;

  for (auto p = result; p != nullptr; p = p->ai_next) {
    char name[NI_MAXHOST];

    err = getnameinfo(p->ai_addr, p->ai_addrlen, name, sizeof(name), nullptr, 0,
                      0);

    if (err) {
      continue;
    }

    if (opts.domain) {
      domains.push_back(name);
    } else {
      lock_guard<mutex> lock(cout_mutex);

      cout << ip << " \t" << name << endl;
    }
  }

  freeaddrinfo(result);

  if (opts.domain) {
    lock_guard<mutex> lock(cout_mutex);

    for (const auto &domain : domains) {
      cout << domain << endl;
    }
  }
}

void run_worker(const vector<string> &ips) {
  for (const auto &ip : ips) {
    do_work(ip);
  }
}

void run_workers(const vector<string> &ips) {
  vector<thread> threads;

  for (int i = 0; i < opts.threads; ++i) {
    threads.emplace_back([&, i] {
      vector<string> sub_ips;

      for (size_t j = i; j < ips.size(); j += opts.threads) {
        sub_ips.push_back(ips[j]);
      }

      run_worker(sub_ips);
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, char *argv[]) {
  parse_options(argc, argv);
  vector<string> ips = read_input();
  run_workers(ips);
  return 0;
}
