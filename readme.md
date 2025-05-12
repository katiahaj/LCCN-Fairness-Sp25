# NS-3 Docker Environment

This project uses Docker to create a consistent environment for running ns-3 simulations with Python.

## Prerequisites

*   [Docker](https://docs.docker.com/get-docker/) installed.

## Building the Docker Image

Navigate to the project root (where `Dockerfile` is) and run:

```bash
docker build -t ns3-app .
```

## Running the Docker Container

This command runs the container and attempts to mount your `workspace` subdirectory (from the directory you run the command from) into `/opt/ns-allinone-3.43/ns-3.43/scratch` inside the container.

### Linux/macOS
```bash
docker run -it --rm -v $(pwd)/workspace:/opt/ns-allinone-3.43/ns-3.43/scratch/workspace ns3-app
```

### Windows (PowerShell)
```bash
docker run -it --rm -v ${PWD}/workspace:/opt/ns-allinone-3.43/ns-3.43/scratch/workspace ns3-app
```

### Windows (CMD)
```bash
docker run -it --rm -v %cd%/workspace:/opt/ns-allinone-3.43/ns-3.43/scratch/workspace ns3-app
```



## Inside Container (Usage)

Your simulation scripts should be placed in the mounted 'scratch/workspace' folder under the NS3 root (/opt/ns-allinone-3.43/ns-3.43). From the NS3 root, simply run:
```bash
ns3 run ./scratch/workspace/<your_script.py>
```