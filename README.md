# Master thesis- LLM in CubeGUI for Performance Analysis in HPC


## Getting started
View the thesis proposal, thesis writeup and the analysis results in the thesis-documentation branch.

The cube files used to test reside in: https://uni-paderborn.sciebo.de/s/8JQjDmZ95TNoRkj

## Build Instructions
To build the plugin, make sure you have **Qt 5.15.13** installed.

git clone git@gitlab.jsc.fz-juelich.de:suresh3/master-thesis-llm-in-cubegui-for-performance-analysis-in-hpc.git

- Change paths in all "CMakeLists.txt" files to fit your system paths.

- Follow the instructions on https://sdlaml.pages.jsc.fz-juelich.de/ai/guides/blablador_api_access/#step-2-obtain-an-api-key to obtain an API key. Steps below in short, refer to the official page for more description:
    - Register on GitLab "https://codebase.helmholtz.cloud/users/sign_in"
    - Create a new Access Token.
    - Copy that Access Token
    - Create a ".blablador_config" file in this directory
    - Paste and the Access token in that file as - BLABLADOR_API_KEY=your_access_token
    - Save it. 

- git checkout main
- To build the plugin, simply run the `build.sh` script located in the project directory:
```bash
bash ./build.sh
```

## Opening a .cubex file
- ALWAYS run the following in your terminal before opening a cubex profile:
export BLABLADOR_API_KEY="your_access_token"
- cube name.cubex

## Name
Master thesis- LLM in CubeGUI for HPC Performance Analysis

## Description
In a nutshell,
The AI Plugin will help CubeGUI detect load imbalance, list reasons for it and provides actionable steps to investigate and fix the imbalance.
The Call Tree JSON and the system tree JSON plugins were created for research purposes and aren't required to run the AI plugin.

## Usage
LLM serves as an intelligent assistant that helps users interpret profiling data, identify potential performance issues and generate hypotheses for further investigation.

## Support
aashikasuresh16@gmail.com

## Authors and acknowledgment
Aashika M Suresh  
Dr. Pavel Saviankou

## Date
03.07.2026
