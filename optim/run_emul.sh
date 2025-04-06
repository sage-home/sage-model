#!/bin/bash

python3 ./sage_emulator_demo.py --mode train \
                            --pso-dir /Users/mbradley/Documents/PhD/SAGE-PSO/sage-model/output/millennium_pso/ \
                            --space-file /Users/mbradley/Documents/PhD/SAGE-PSO/sage-model/optim/space.txt \
                            --constraints SMF_z0 \
                            --method random_forest \
                            --output-dir ../output/emulator_output

python3 ./sage_emulator_demo.py --mode predict \
                            --space-file /Users/mbradley/Documents/PhD/SAGE-PSO/sage-model/optim/space.txt \
                            --constraints SMF_z0\
                            --output-dir ../output/emulator_output

python3 ./sage_emulator_integration.py -c /Users/mbradley/Documents/PhD/SAGE-PSO/sage-model/input/millennium.par \
                                   -S /Users/mbradley/Documents/PhD/SAGE-PSO/sage-model/optim/space.txt \
                                   -v 0 \
                                   -e ../output/emulator_output/emulators \
                                   -s prescreening \
                                   -m 50 \
                                   -x SMF_z0 \
                                   -t student-t
            