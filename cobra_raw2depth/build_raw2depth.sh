sudo rm -r build
mkdir build
cd build
cmake CMAKE_BUILD_TYPE=Release ..
make -j6
sudo make install
sudo ../front-end-cpp/frontend_postinstall.sh
systemctl daemon-reload
