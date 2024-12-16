# README #

## What is this repository for? ###

This repository contains two separate software systems:
1. A REST HTTP API in the form of a Python library for the Lumotive LIDAR system. It also includes unit and integration tests
2. A Javascript front end based on React that runs in a user's browser that acts as a graphical user interface (GUI) for the Lumotive LIDAR system

The two software systems are discussed separately in more detail in the rest of this document.

## REST API

The HTTP REST API is designed using the `flask-restful` plugin for simple REST compliance.

### Anatomy of the REST API

The REST API project is contained a directory called `cobra_lidar_api`. This is the project root. Note that in this document, when we refer to files within the project, we specify them relative to the project root. The pip installable Python module source code resides the `cobra_lidar_api` directory within the project root.

All resources are defined and routed in `cobra_lidar_api/api.py`. There are additional routings for the index and message queue found in `cobra_lidar_api/web_server.py`. Schema used for loading user data and dumping system data, and associated helper methods, can be found in `cobra_lidar_api/api_schema.py`.

### Where the API runs

The REST API can be run either on an NXP Compute Board (NCB) or a user's PC running the Ubuntu operating system. It may also work on Windows. In practice, the REST API code is used in the following places:

| System | Form of code | Purpose |
|--------|--------------|---------|
| NCB    | Release library | Release image |
| NCB    | Source code     | For development on the NCB |
| Ubuntu PC | Source code  | For running unit and integration tests |

We go into further details about each of these scenarios in the next few sections.

#### REST API on the NCB running the release software

When the NCB is running the release software, the LIDAR API code can be found at `/usr/lib/python3.10/site-packages/cobra_lidar_api`.  The code is in the form of an installed Python library. To support multiple concurrent requests, we employ the Gunicorn Python HTTP server with three worker threads. A systemd script called `cb_api.service` starts up the server.

#### REST API on the NCB running from source code

You can put the REST API source code on the NCB and edit it in place. To do this, place the source in the home directory (i.e., `/home/root`) in a directory called `cobra_lidar_api`. The project root will then be `/home/root/cobra_lidar_api`. As mentioned earlier, another `cobra_lidar_api` directory containing the python source code files (e.g. `api.py`) resides within the project root.

Once the source is in the right place you can install the library:

    cd /home/root/cobra_lidar_api
    python3.10 -m pip install --no-deps -e .
    systemctl restart cb_api

The systemd script that starts up the REST API in the release image can start up the development version as long as the project root is the `/home/root/cobra_lidar_api` directory.

You should also move the UI from its original location in `/usr/lib/python3.10/site-packages/cobra_lidar_api`.

    mv /usr/lib/python3.10/site-packages/cobra_lidar_api/m30_webapp /home/root/cobra_lidar_api/cobra_lidar_api

During testing, whenever you make changes to your python code you will need to restart the API. You do this with the following command:

    systemctl restart cb_api

##### Testing the API on NCB with Flask

You can test your UI using Flask itself and bypass Gunicorn. This configuration has the advantage during development that you don't have to restart the API service every time you make a change to the API code. We don't recommend using Flask directly for deployment scenarios because it does not support multiple concurrent HTTP requests.

To start the API using Flask do the following:

    cd /home/root/cobra_lidar_api/cobra_lidar_api
    systemctl stop cb_api
    python3.10 wsgi.py

To access the API you will use port 5001 of the NCB's IP address. For example, if you want to get the LIDAR state you can issue the following from your PC:

    curl http://10.20.30.40:5001/state

To turn off the server, type Ctrl-C in the shell where you executed the wsgi application.

#### REST API on a PC running Ubuntu

On a PC running Ubuntu you can run unit tests and integration tests.

Both unit and integration tests are meant to be performed on a PC connected to the NCB.  Unit tests mock hardware interactions entirely.  Integration tests use a physical NCB and communicate solely through the API.

When running tests, you need to have this package and its dependencies installed.  Additionally, you will need `pytest` and `pytest-flask` (unit tests only) installed inside your testing environment.

You can put the source code anywhere, but when you need to install the library you need to install it editable and point to the project root. We recommend you use a Python virtual environment.

    cd <project_root>
    pip install -e .

To run unit tests, execute the following from the project root

    pytest tests/unit
    pytest tests/integration --hostname=10.20.30.40

You may need to adjust the hostname depending on how your PC is networked to your NCB.

## Javascript UI

The user interface runs in the client's web browser and communicates with the REST API. It is built with React, and like other React apps, it comes in two forms: the source code and a compressed version for deployment. The following table shows where the user interface code resides and in which form.

|System | Form | Location | Purpose |
|-------|------|----------|---------|
|NCB    | Deployment | `/usr/lib/python3.10/site-packages/cobra_lidar_api` | Release image |
|Ubuntu PC | Source | `<project_root>/src` | Development |
|Ubuntu PC | Deployment | `<project_root>/m30_webapp` | Build products |

### Testing the UI from your Ubuntu PC

You can test the UI using its source form on an Ubuntu PC and use your PC's browser by executing the following command:

    cd <project_root>
    export REACT_APP_BASE_URL=http://10.20.30.40/
    npm start

This will cause node to host the UI and start it up on your default browser. If you modify and save any of the files in the `src` directory, the server will automatically detect the changes in real time and serve them, reporting errors to the shell where you started the UI. If you need to stop hosting the UI, just type Ctrl-C in the shell where you executed `npm start`.

If you are testing the API using Flask and not Gunicorn, your port number is 5001 instead of 80. In that case you use a different export:

    export REACT_APP_BASE_URL=http://10.20.30.40:5001/

### Converting the source code to deployed form

Converting the source code to the deployed form is best done on a PC running Ubuntu, although it can likely be done under Windows as well. This is how you do it in Ubuntu.

1. Install [Node.js](https://nodejs.org/en/download/) if you don't have it already. You will need versions `node >= 16.0.0` and `npm >= 5.0.0`.

2. Install UI dependencies locally.

       cd <project_root>
       npm install

3. Build the deployment image

       npm run build

4. Move the build products to the NCB. The build products are located in `<project_root>/cobra_lidar_api/m30_webapp`. From the project root:

       tar czf m30_webapp.tgz -C cobra_lidar_api m30_webapp
    
   Copy the `.tgz` file to the `/tmp` directory of the NCB. 

       scp m30_webapp.tgz root@10.20.30.40:/tmp

5. If you are running the API from the release location in `/usr/lib/python3.10/site-packages/cobra_lidar_api` execute the following on the NCB:

       tar xf /tmp/m30_webapp.tgz -C /usr/lib/python3.10/site-packages/cobra_lidar_api

6. If you are running the API from its development location in `/home/root/cobra_lidar_api` execute the following on the NCB:

       tar xf /tmp/m30_webapp.tgz -C /home/root/cobra_lidar_api/cobra_lidar_api

## Additional notes

### Sample request code
Sample code for testing the API using Python's ``request`` module can be found in ``scripts/api_sample.py``.  Note that this code is merely an example and does not have to be executed or imported directly. The script can be used as a CLI for the API, but is pretty limited. It is recommended you integrate a similar API object into your codebase for automated communication.

### Postman script
The API is documented using a Postman script in the resources directory of the project root. You can import this file directly into Postman.

### Why is there no web server?

We use Gunicorn to directly serve up the API. If the device is on the internet and subject to attack from the world, you would want to put an industrial strength web server in front of Gunicorn (Nginx is a possibility). However we don't recommend putting this device on the internet so we don't use a web server.
