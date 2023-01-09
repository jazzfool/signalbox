#include "tracker.h"

int main() {
    tracker{}.create().run_loop().destroy();
}