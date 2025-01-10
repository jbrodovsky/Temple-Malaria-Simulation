# Temple University - Boni Lab - Spatial Malaria Simulation

[Temple University](https://www.temple.edu/) - [Boni Lab](http://mol.ax/)

---

## Overview

This repository contains the working codebase for the Spatial Malaria Simulation (MaSim) under development by the Boni Lab at Temple University. The codebase was originally forked from [maciekboni/PSU-CIDD-Malaria-Simulation](https://github.com/maciekboni/PSU-CIDD-Malaria-Simulation) and was detached in perpetration of future development.

A comprehensive [technical manual](manual/manual.pdf) (PDF) for the simulation can be found in the [manual](manual/) directory which includes comprehensive directions for working with the simulation; however, a basic reference is provided in the Wiki. Stable code specific to publications are maintained in repositories under the [Boni Lab on GitHub](https://github.com/bonilab). 

The simulation has been tested to run on Windows 10, Windows Subsystem for Linux (Ubuntu), and Red Hat 7.9. The majority of development is performed on under Linux so building and running under Windows may be impacted.  While basic simulations are possible on desktop computing environments, regional and national scale simulations require advanced computing environments with access to 64 GB of RAM or more. Sample configuration files can be found under [documentation/input/](documentation/input), and examination of `simple.yml` or `spatial.yml` is recommended after working with the demonstration configuration in [documentation/demo/](documentation/demo/).

## Installation

The simulation is semi-portable and is currently run in a Linux environment using a [Development Container](https://containers.dev/) for two reasons. First, there are some system-level dependancies (ex: `cmake` and `flex`) that some users might not use from project to project as compared to a dependency like `git`. Second, the simulation is written in C++ and uses `vcpkg` for package management. `vcpkg` additionally has some system-level dependencies that are not always present on a system and users may not want to install them globally.

The recommended way of building and running the simulation is through Visual Studio Code's [Remote - Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension. This extension allows for the development of the simulation in a containerized environment that is consistent across all developers. The `Dockerfile` and `devcontainer.json` files are included in the repository for this purpose. This setup requires that [Docker](https://www.docker.com/) be installed on the host machine. The simplest method is to download and install Docker Desktop.

With those prerequisites met, the following steps can be taken to build and run the simulation:

1. Ensure that Docker Desktop is running on the host machine.
2. Clone the repository to the host machine.
3. Open the repository in Visual Studio Code.
4. Open the Command Palette (Ctrl+Shift+P) and select "Dev Containers: Reopen in Container".

After this the Docker will pull the image and build the container and then subsequently the simulation will be built.

If you are not using Visual Studio Code, the Dockerfile can still be used to build the container. To build the simulation once inside the running container, please use the script `scripts/build.sh`.

## Operation

The executable is configured to read everything from a relative path to where the binary is located. The current operatting practice is to navigate to the `build/bin` directory and run the simulation from there. Data files (`.asc`, `input.yml`, `.csv`, etc.) should be placed in the same directory as the binary or in a subdirectory. The simulation will create output files in the same directory as the binary. In the `input.yml` file, file paths to `.asc` files should be relative to the location of the binary.

An example call looks like this:

```bash
./MaSim -i input.yml -r SQLiteDistrictReporter -o ./output
```

## Command Line Arguments

The following commands are available from the simulation:
<pre>
-c / --config     Configuration file, variant flag 
-h / --help       Display this help menu
-i / --input      Configuration file, preferred flag
-j                The job number for this replicate
-l / --load       Load genotypes to the database and exit
-o                The path for output files, default is the current directory
-r                The reporter type to use, multiple supported when comma delimited
-s                The study number to associate with the configuration

--dump            Dump the movement matrix as calculated
--im              Record individual movement detail
--lg              List the possible genotypes and their internal id values
--lr              List the possible data reporters
--mc              Record the movement between cells, cannot run with --md
--md              Record the movement between districts, cannot run with --mc

--v=[int]         Sets the verbosity of the logging, default zero
</pre>

Use of either the `-c` or `-i` switch with an appropriate YAML file is required. When the `-r` switch is not supplied the simulation defaults to the `DbReporter`; however, with the `-r` switch the reporters listed using the `--lr` switch can be used instead.

