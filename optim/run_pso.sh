#!/bin/bash

# Define arguments for main.py
CONFIG_PATH="/fred/oz004/mbradley/SAGE-PARAMS/sage-model/input/millennium.par"
BASE_PATH="/fred/oz004/mbradley/SAGE-PARAMS/sage-model/sage"
OUTPUT_PATH="/fred/oz004/mbradley/SAGE-PARAMS/sage-model/output/millennium_pso_alphas_SMFz0"
PARTICLES=14
ITERATIONS=30
TEST="student-t"
CONSTRAINTS="SMF_z0"
AGE_ALIST_FILE_MINI_UCHUU='/fred/oz004/msinha/simulations/uchuu_suite/miniuchuu/mergertrees/u400_planck2016_50.a_list'
AGE_ALIST_FILE_MINI_MILLENNIUM="/fred/oz004/mbradley/SAGE-PARAMS/sage-model/input/millennium/trees/millennium.a_list"
BOXSIZE=62.5
SIM_MINI_UCHUU=0
SIM_MINI_MILLENNIUM=1
VOL_FRAC=1.0
OMEGA0=0.25 
H0=0.73
CSVOUTPUT="/fred/oz004/mbradley/SAGE-PARAMS/sage-model/output/millennium_pso_alphas_SMFz0/params_z0.csv"
SPACEFILE="/fred/oz004/mbradley/SAGE-PARAMS/sage-model/optim/space_alphas.txt"
ACCOUNT="oz004"

ml purge
ml restore basic


python3 ./main.py \
  -c "$CONFIG_PATH" \
  -b "$BASE_PATH" \
  -o "$OUTPUT_PATH" \
  -s "$PARTICLES" \
  -m "$ITERATIONS" \
  -t "$TEST" \
  -x "$CONSTRAINTS" \
  -csv "$CSVOUTPUT" \
  --age-alist-file "$AGE_ALIST_FILE_MINI_MILLENNIUM" \
  --sim "$SIM_MINI_MILLENNIUM"\
  --boxsize "$BOXSIZE" \
  --vol-frac "$VOL_FRAC" \
  --Omega0 "$OMEGA0" \
  --h0 "$H0" \
  -S "$SPACEFILE" 