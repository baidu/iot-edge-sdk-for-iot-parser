#include <device_management.h>

class SmartBubble {
private:
    int lumen;
public:
    int getLumen();
    void setLumen(int desired);
};

int SmartBubble::getLumen() {
    return lumen;
}

void SmartBubble::setLumen(int desired) {
    
}

int main() {
    device_management_init();
    device_management_fini();
}

