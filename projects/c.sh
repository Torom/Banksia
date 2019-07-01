rm banksia

g++ -std=c++11 -c ../src/3rdparty/json/*.cpp -O3 -DNDEBUG
g++ -std=c++11 -c ../src/3rdparty/process/process.cpp ../src/3rdparty/process/process_unix.cpp -O3 -DNDEBUG
g++ -std=c++11 -c ../src/base/*.cpp -O3 -DNDEBUG
g++ -std=c++11 -c ../src/chess/*.cpp -O3 -DNDEBUG
g++ -std=c++11 -c ../src/game/*.cpp -O3 -DNDEBUG
g++ -std=c++11 -c ../src/*.cpp -O3 -DNDEBUG

g++ -o banksia *.o
rm *.o
./banksia

