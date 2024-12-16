**********************
Python R2D
**********************

This project is the python equivalent to C++ R2D. It is mainly used for vector matching and R&D purposes.

Project Structure
-------------------
Python R2D is completely contained in the development folder of the cobra_raw2depth repo. The folder organization is:

* **colormaps**: saved custom colormaps for display;
* **docs**: holds this documentation;
* **legacy**: contains all of the code developed by Mike Albert;
* **misc**: useful experiments to keep track of that are **not** part of python R2D;
* **output** (auto-generated): contains the output files of python R2D, typically used for vector matching;
* **src**: contains python R2D source files. This is where code development happens, not where code usage happens;
* **testbenches**: contains the testbenches (user interfaces) used to control python R2D. Head to this folder to start using python R2D;
* **tools**: useful scripts;

Working directory
-------------------
This code assumes that the working directory is **cobra_raw2depth**.

Requirements
---------------
A requirements.txt file is provided at the root of **cobra_raw2depth**. You will also need to clone [Lumotive's utilities package](https://bitbucket.org/lumotive/utilities/src/master/). Clone it at the same level than your cobra_raw2depth folder.

Quick start
--------------
Using git, clone the cobra_raw2depth project and the utilities project at the same folder level.

Using PyCharm:

1. Select the cobra_raw2depth folder and open it as a project;
2. Upon first loading, you should be prompted with a 'Creating Virtual Environment Window';
    1. For **Location**, choose cobra_raw2depth/venv;
    2. For **Base Interpreter**, select the path leading to the python 3.7+ interpreter that you want to use;
    3. For **Dependencies**, make sure the path points to cobra_raw2depth/requirements.txt;
    4. Click OK to generate the python virtual environment and **wait until all tasks are completed successfully**;
3. From the project tab in pycharm, head to the development folder and then to the testbenches folder to open up M20_simple_testbench.py;
4. Make sure that you have valid raw data somewhere (by default I place the raw data at the same level than the cobra_raw2depth folder) and point the testbench to that raw data;
5. Run that testbench, it should show you a depth map.

Usage
-------
As mentioned in **Project Structure**, this code should be used by creating testbenches or using an already existing one. To
get started, have a look at the **M20_simple_testbench.py** file for a simple example.

Important testbenches
-----------------------
1. **M20_simple_testbench.py**: Default testbench for single frame processing. Used to dump data for vector matching with R2D.
2. **M20_time_domain_boxcar_tv.py**: A good example of how to use the DSP device with consecutive frames.
3. **M20_stripe_mode_single_frame.py**: The equivalent to M20_simple_testbench when using stripe mode.