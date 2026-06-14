# Master thesis- LLM in CubeGUI for HPC Performance Analysis

## Datasets and experiments

The "Datasets" folder contains some of the experiments and relevant cubex files you can use to test the plugin.

## Build Instructions for the AI Plugin
To build the AI-Plugin, make sure you have **Qt 5.15.13** installed.

HTTPS:
git clone https://github.com/aashika116/Thesis-datasets-and-additional-data.git OR 

SSH:
git clone git@github.com:aashika116/Thesis-datasets-and-additional-data.git

- Change paths in all "CMakeLists.txt" files to fit your system paths.

- Follow the instructions on https://sdlaml.pages.jsc.fz-juelich.de/ai/guides/blablador_api_access/#step-2-obtain-an-api-key to obtain an API key. Steps below in short, refer to the official page for more description:
    - Register on GitLab "https://codebase.helmholtz.cloud/users/sign_in"
    - Create a new Access Token.
    - Copy that Access Token
    - Create a ".blablador_config" file in this directory
    - Paste and the Access token in that file as - BLABLADOR_API_KEY=your_access_token
    - Save it. 

- git checkout cubegui-plugins
- To build the plugin, simply run the `build.sh` script located in the project directory:
```bash
bash ./build.sh
```

## Opening a .cubex file
- Run the following in your terminal before opening a cubex profile:
export BLABLADOR_API_KEY="your_access_token"
- cube name.cubex

## Name
Master thesis- LLM in CubeGUI for Performance Analysis in HPC

## Description/Short how it works
In progress.
In a nutshell,
The Plugin will help CubeGUI detect and the LLM will list the reasons for load imbalance+investigation+fix.
The Call tree and System Tree JSON plugins were developed for initial LLM testing.

## Usage
LLM in CubeGUI is designed to transform the tool into an intelligent analysis assistant capable of interpreting, reasoning and explaining performance data (load imbalance to begin with).

## Support
aashika.suresh@mail.uni-paderborn.de

## Author
Aashika Suresh
