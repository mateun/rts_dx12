

#include <windows.h>

class Timer {

    public: 
        Timer();
        void start();
        void stop();
        float diffInSeconds();

    private:
          LARGE_INTEGER frequency;
          LARGE_INTEGER currentCounter;
          LARGE_INTEGER stopCounter;
};