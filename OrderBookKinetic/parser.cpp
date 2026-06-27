#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#pragma pack(push, 1)
struct Order {
  uint64_t timestamp;
  int64_t price;
  uint32_t volume;
  char buySell;
};
#pragma pack(pop)

class RingLoop {
private:
  static constexpr int capacity = 1024;
  Order buffer[capacity]{};

  std::atomic<size_t> head = 0;
  std::atomic<size_t> tail = 0;

public:
  bool push(const Order &order) {
    size_t current_head = head.load(std::memory_order_relaxed);
    size_t current_tail = tail.load(std::memory_order_relaxed);

    if ((current_head + 1 - current_tail) > capacity) {
      return false;
    }

    buffer[current_head & (capacity - 1)] = order;
    head.store((current_head + 1), std::memory_order_release);
    return true;
  }

  bool pop(Order &order) {
    size_t current_head = head.load(std::memory_order_acquire);
    size_t current_tail = tail.load(std::memory_order_relaxed);

    if (current_tail == current_head) {
      return false;
    }
    order = buffer[current_tail & (capacity - 1)];
    tail.store(current_tail + 1, std::memory_order_relaxed);
    return true;
  }
};

class OrderBook {
public:
  static constexpr int64_t MIN_PRICE = 10000;
  static constexpr int64_t MAX_PRICE = 11000;
  static constexpr size_t PRICE_RANGE = MAX_PRICE - MIN_PRICE + 1;
  struct Pricelevel {
    int64_t bidvolume = 0;
    int64_t askvolume = 0;
  };

  void updateBook(const Order &order) {
    if (order.price >= MIN_PRICE && order.price <= MAX_PRICE) {
      size_t index = order.price - MIN_PRICE;
      if (order.buySell == 'B') {
        book[index].bidvolume += order.volume;
      } else if (order.buySell == 'S') {
        book[index].askvolume += order.volume;
      }
    }
  }
  double calculatePressure() {
    int64_t TotalAskVolume = 0;
    int64_t TotalBidsVolume = 0;
    for (int i = 0; i < PRICE_RANGE; i++) {
      TotalAskVolume += book[i].askvolume;
      TotalBidsVolume += book[i].bidvolume;
    }
    int64_t TotalVolume = TotalAskVolume + TotalBidsVolume;
    if (TotalVolume > 0) {
      return static_cast<double>(TotalBidsVolume) /
             (TotalBidsVolume + TotalAskVolume);
    }
    return static_cast<double>(0.5);
  }

private:
  Pricelevel book[PRICE_RANGE]{};
};

class UDPReceiver {
private:
  int sockfd;

public:
  UDPReceiver(int port) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    int opt = 1;
    int rcvbuf = 8 * 1024 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &rcvbuf,
               sizeof(rcvbuf));

    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(sockaddr_in));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&server_addr),
             sizeof(server_addr)) < 0) {
      std::cerr << "Fatal Error: Port 8080 is already in use.\n";
      exit(1);
    }
  };
  void startListening(RingLoop &ring) {
    char packet_buffer[21];
    while (true) {
      recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, nullptr,
               nullptr);
      Order *order_ptr = reinterpret_cast<Order *>(packet_buffer);
      ring.push(*order_ptr);
    }
  }
};

void drawPressureBar(double raw_pressure) {
  int pressure = static_cast<int>((raw_pressure - 0.5) * 40.0);

  if (pressure > 20)
    pressure = 20;
  if (pressure < -20)
    pressure = -20;

  const std::string RED = "\033[31m";
  const std::string GREEN = "\033[32m";
  const std::string RESET = "\033[30m";

  if (pressure < 0) {
    int left_spaces = 20 - std::abs(pressure);
    std::string spaces(left_spaces, ' ');
    std::string arrows(std::abs(pressure), '<');
    std::cout << spaces << RED << arrows << RESET << "|\n";
  } else if (pressure > 0) {
    std::string spaces(20, ' ');
    std::string arrows(pressure, '>');
    std::cout << spaces << "|" << GREEN << arrows << RESET << "\n";
  } else {
    std::string spaces(20, ' ');
    std::cout << spaces << "|\n";
  }
}

int main() {
  RingLoop ring;
  OrderBook book;
  UDPReceiver receiver(8080);

  std::thread network_threrad(&UDPReceiver::startListening, &receiver,
                              std::ref(ring));

  while (true) {
    Order temp_order;

    if (ring.pop(temp_order)) {
      book.updateBook(temp_order);
      drawPressureBar(book.calculatePressure());
    }
  }
  if (network_threrad.joinable()) {
    network_threrad.join();
  }

  return 0;
}
