#!/bin/bash

# Script para correr el contenedor con Docker
docker run -it --rm -v "$(pwd)":/app agodio/itba-so-multi-platform:3.0
