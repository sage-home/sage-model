#!/bin/bash

import argparse
import logging
import math
import multiprocessing
import os
import sys
import time

def _abspath(p):
    return os.path.normpath(os.path.abspath(p))

sys.path.insert(0, _abspath(os.path.join(__file__, '..', '..', 'output')))


import analysis
import common
import constraints
import execution
import pso
import glob
import diagnostics2


logger = logging.getLogger('main')

def setup_logging(outdir):
    log_fname = os.path.join(outdir, 'sage_pso.log')
    fmt = '%(asctime)-15s %(name)s#%(funcName)s:%(lineno)s %(message)s'
    fmt = logging.Formatter(fmt)
    fmt.converter = time.gmtime
    
    # Set root logger level
    logging.root.setLevel(logging.INFO)
    
    # Console handler
    console_handler = logging.StreamHandler(stream=sys.stdout)
    console_handler.setFormatter(fmt)
    logging.root.addHandler(console_handler)
    
    # File handler
    file_handler = logging.FileHandler(log_fname)
    file_handler.setFormatter(fmt)
    logging.root.addHandler(file_handler)
    
    # Ensure all loggers inherit these settings
    logging.getLogger('diagnostics').setLevel(logging.INFO)
    logging.getLogger('main').setLevel(logging.INFO)

def get_required_snapshots(constraints_str):
    """Get all unique snapshots needed for constraints"""
    # Map of constraint classes to their snapshots 
    snapshot_map = {
        'SMF_z0': [63],
        'SMF_z02': [43],
        'SMF_z05': [38],
        'SMF_z08': [34],
        'SMF_z10': [40], 
        'SMF_z11': [38],
        'SMF_z15': [27],
        'SMF_z20': [32],
        'SMF_z24': [20],
        'SMF_z31': [16],
        'SMF_z36': [14],
        'SMF_z46': [11],
        'SMF_z57': [9],
        'SMF_z63': [8],
        'SMF_z77': [6],
        'SMF_z85': [5],
        'SMF_z104': [3],
        'BHMF_z0': [63],
        'BHMF_z20': [32],
        'BHBM_z0': [63],
        'BHBM_z20': [32],
        'HSMR_z0': [63],
        'SMF_red_z0': [63],
        'SMF_blue_z0': [63],
        'SMF_Color_z0': [63],
        'SMD_evolution': [63],
        'TARGET_SMF_z0': [63],
        'TARGET_SMF_z05': [38],
        'TARGET_SMF_z10': [40],
        'TARGET_SMF_z20': [32],
        'TARGET_SMF_z31': [16],
        'TARGET_SMF_z46': [11]
    }
    
    snapshots = set()
    print(f"Parsing constraints string: {constraints_str}")
    for constraint in constraints_str.split(','):
        # Remove any weight/domain specifications
        base_constraint = constraint.split('(')[0].split('*')[0]
        print(f"Processing constraint: {constraint}")
        print(f"Base constraint: {base_constraint}")
        if base_constraint in snapshot_map:
            print(f"Found snapshot mapping: {snapshot_map[base_constraint]}")
            snapshots.update(snapshot_map[base_constraint])
        else:
            print(f"Warning: No snapshot mapping found for {base_constraint}")
    
    result = sorted(list(snapshots))
    print(f"Final snapshots list: {result}")
    return result

def cleanup_files(opts):
    """Clean up dump files and track files after PSO run"""
    
    # Define patterns for files to delete
    patterns = {
        'smf_dumps': os.path.join(opts.outdir, 'SMF_z*_dump.txt'),
        'bhmf_dump': os.path.join(opts.outdir, 'BHMF_z*_dump.txt'),
        'bhbm_dump': os.path.join(opts.outdir, 'BHBM_z*_dump.txt'),
        'hsmr_dump': os.path.join(opts.outdir, 'HSMR_z*_dump.txt')
    }

    # Delete dump files
    for pattern_name, pattern in patterns.items():
        matching_files = glob.glob(pattern)
        for file_path in matching_files:
            try:
                os.remove(file_path)
                print(f"Deleted {os.path.basename(file_path)}")
            except OSError as e:
                print(f"Error deleting {os.path.basename(file_path)}: {e}")
    """
    # Clean up tracks folder
    tracks_folder = os.path.join(opts.outdir, 'tracks')
    if os.path.exists(tracks_folder):
        for ext in ['*.npy', '*.par']:
            for file in glob.glob(os.path.join(tracks_folder, ext)):
                try:
                    os.remove(file)
                except OSError as e:
                    print(f"Error deleting {os.path.basename(file)}: {e}")
    """
    # Clean up tracks folder
    tracks_folder = os.path.join(opts.outdir, 'tracks')
    par_folder = opts.outdir
    
    # Define extensions to clean up
    extensions = ['.npy', '.par']
    """
    # Clean up tracks folder
    if os.path.exists(tracks_folder) and os.path.isdir(tracks_folder):
        for ext in extensions:
            for file in glob.glob(os.path.join(tracks_folder, f'*{ext}')):
                try:
                    os.remove(file)
                except OSError as e:
                    print(f"Error deleting {os.path.basename(file)}: {e}")
    """
    # Clean up par folder
    if os.path.exists(par_folder) and os.path.isdir(par_folder):
        for ext in extensions:
            for file in glob.glob(os.path.join(par_folder, f'*{ext}')):
                try:
                    os.remove(file)
                except OSError as e:
                    print(f"Error deleting {os.path.basename(file)}: {e}")

def main():

### HEAVILY modified to be SAGE specific
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', help='Configuration file used as the basis for running sage', type=_abspath)
    parser.add_argument('-v', '--subvolumes', help='Comma- and dash-separated list of subvolumes to process', default='0')
    parser.add_argument('-b', '--sage-binary', help='The shark binary to use, defaults to either "sage" or "../sage"',
                        default=None, type=_abspath)
    parser.add_argument('-o', '--outdir', help='Auxiliary output directory, defaults to .', default=_abspath('.'),
                        type=_abspath)
    parser.add_argument('-k', '--keep', help='Keep temporary output files', action='store_true')
    parser.add_argument('-sn', '--snapshot', help='Comma-separated list of snapshot numbers to analyze', 
                   type=lambda x: [int(i) for i in x.split(',')], default=None)
    parser.add_argument('--sim', help='Simulation to use (0=miniUchuu, 1=miniMillennium, 2=MTNG)', 
                   type=int, default=0)
    parser.add_argument('--boxsize', help='Size of the simulation box in Mpc/h', 
                    type=float, default=400.0)
    parser.add_argument('--vol-frac', help='Volume fraction of the simulation box', 
                    type=float, default=0.0019)
    parser.add_argument('--age-alist-file', help='Path to the age list file, match with .par file',
                   default=None, type=_abspath)
    parser.add_argument('--Omega0', help='Omega0 value for the simulation', 
                    type=float, default=0.3089)
    parser.add_argument('--h0', help='H0 value for the simulation', 
                    type=float, default=0.677400)
###

    pso_opts = parser.add_argument_group('PSO options')
    pso_opts.add_argument('-s', '--swarm-size', help='Size of the particle swarm. Defaults to 10 + sqrt(D) * 2 (D=number of dimensions)',
                          type=int, default=None)
    pso_opts.add_argument('-m', '--max-iterations', help='Maximum number of iterations to reach before giving up, defaults to 20',
                          default=10, type=int)
    pso_opts.add_argument('-S', '--space-file', help='File with the search space specification, defaults to space.txt',
                          default='space.txt', type=_abspath)
    pso_opts.add_argument('-t', '--stat-test', help='Stat function used to calculate the value of a particle, defaults to student-t',
                          default='student-t', choices=list(analysis.stat_tests.keys()))
    pso_opts.add_argument('-x', '--constraints', default='BHMF,SMF_z0,BHBM',
                          help=("Comma-separated list of constraints, any of BHMF, SMF_z0 or BHBM, defaults to 'BHMF,SMF_z0,BHBM'. "
                                "Can specify a domain range after the name (e.g., 'SMF_z0(8-11)')"
                                "and/or a relative weight (e.g. 'BHMF*6,SMF_z0(8-11)*10)'"))
    pso_opts.add_argument('-csv', '--csv-output', help='Path to save PSO results as CSV file. If not specified, no CSV will be generated.',
                      type=_abspath, default=None)

### 
    hpc_opts = parser.add_argument_group('HPC options')
    hpc_opts.add_argument('-H', '--hpc-mode', help='Enable HPC mode', action='store_true')
    hpc_opts.add_argument('-C', '--cpus', help='Number of CPUs per sage instance', default=1, type=int)
    hpc_opts.add_argument('-M', '--memory', help='Memory needed by each sage instance', default='1500m')
    hpc_opts.add_argument('-N', '--nodes', help='Number of nodes to use', default=None, type=int)
    hpc_opts.add_argument('-a', '--account', help='Submit jobs using this account', default=None)
    hpc_opts.add_argument('-q', '--queue', help='Submit jobs to this queue', default=None)
    hpc_opts.add_argument('-w', '--walltime', help='Walltime for each submission, defaults to 1:00:00', default='1:00:00')
    hpc_opts.add_argument('-u', '--username', help='Username for SLURM job submission', default=None)
###

    opts = parser.parse_args()

    if not opts.config:
        parser.error('-c option is mandatory but missing')

    if opts.snapshot:
        snapshots = opts.snapshot
    else:
        snapshots = get_required_snapshots(opts.constraints)
        opts.snapshot = snapshots

    # Create the output directory if it doesn't exist
    os.makedirs(opts.outdir, exist_ok=True)

    if opts.sage_binary and not common.has_program(opts.sage_binary):
        parser.error("SAGE binary '%s' not found, specify a correct one via -b" % opts.sage_binary)
    elif not opts.sage_binary:
        for candidate in ['sage', '../sage']:
            if not common.has_program(candidate):
                continue
            opts.sage_binary = _abspath(candidate)
            break
        if not opts.sage_binary:
            parser.error("No SAGE binary found, specify one via -b")

#    _, _, _, redshift_file = common.read_configuration(opts.config)
#    redshift_table = common._redshift_table(redshift_file)
    subvols = common.parse_subvolumes(opts.subvolumes)

    setup_logging(opts.outdir)

    opts.constraints = constraints.parse(opts.constraints, snapshot=opts.snapshot,
                                    boxsize=opts.boxsize,
                                    sim=opts.sim,
                                    vol_frac=opts.vol_frac,
                                    age_alist_file=opts.age_alist_file,
                                    Omega0=opts.Omega0, h0=opts.h0,
                                    output_dir=opts.outdir)
#    for c in opts.constraints:
#        c.redshift_table = redshift_table

    # Read search space specification, which is a comma-separated multiline file,
    # each line containing the following elements:
    #
    # param_name, plot_label, is_log, lower_bound, upper_bound
    space = analysis.load_space(opts.space_file)

    ss = opts.swarm_size
    if ss is None:
        ss = 10 + int(2 * math.sqrt(len(space)))

    args = (opts, space, subvols, analysis.stat_tests[opts.stat_test])

    if opts.hpc_mode:
        procs = 0
        f = execution.run_sage_hpc
#    else:
#        n_cpus = multiprocessing.cpu_count()
#        procs = min(n_cpus, ss)
#        f = execution.run_shark
    else:
        n_cpus = multiprocessing.cpu_count()
        print('seeing', n_cpus, 'CPUs')
        procs = min(n_cpus, ss)        
        f = execution.run_sage



    logger.info('-----------------------------------------------------')
    logger.info('Runtime information')
    logger.info('    SAGE binary: %s', opts.sage_binary)
    logger.info('    Base configuration file: %s', opts.config)
    logger.info('    Subvolumes to use: %r', subvols)
    logger.info('    Output directory: %s', opts.outdir)
    logger.info('    Simulation Type: %d (0=miniUchuu, 1=miniMillennium, 2=MTNG)', opts.sim)
    logger.info('    Box Size: %.1f', opts.boxsize)
    logger.info('    Volume Fraction: %.4f', opts.vol_frac)
    logger.info('    Age List File: %s', opts.age_alist_file if opts.age_alist_file else 'Using default')
    logger.info('    Omega0: %.4f', opts.Omega0)
    logger.info('    h0: %.4f', opts.h0)
    logger.info('    Keep temporary output files: %d', opts.keep)
    logger.info('    Snapshot Number: %s', opts.snapshot)
    logger.info("PSO information:")
    logger.info('    Search space parameters: %s', ' '.join(space['name']))
    logger.info('    Swarm size: %d', ss)
    logger.info('    Maximum iterations: %d', opts.max_iterations)
    logger.info('    Lower bounds: %r', space['lb'])
    logger.info('    Upper bounds: %r', space['ub'])
    logger.info('    Test function: %s', opts.stat_test)
    logger.info('Constraints:')
    for c in opts.constraints:
        logger.info('    %s', c)
    logger.info('    CSV Output Path: %s', opts.csv_output if opts.csv_output else 'Not specified')
    logger.info('HPC mode: %d', opts.hpc_mode)
    if opts.hpc_mode:
        logger.info('    Account used to submit: %s', opts.account if opts.account else '')
        logger.info('    Queue to submit: %s', opts.queue if opts.queue else '')
        logger.info('    Walltime per submission: %s', opts.walltime)
        logger.info('    CPUs per instance: %d', opts.cpus)
        logger.info('    Memory per instance: %s', opts.memory)
        logger.info('    Nodes to use: %s', opts.nodes)
        logger.info('    Username to use: %s', opts.username if opts.username else '')
    """
    raw_input = input
    while True:
        answer = raw_input('\nAre these parameters correct? (Yes/no): ')
        if answer:
            if answer.lower() in ('n', 'no'):
                logger.info('Not starting PSO, check your configuration and try again')
                return
            print("Please answer 'yes' or 'no'")
            continue
        break
    """
    # Directory where we store the intermediate results
    tracksdir = os.path.join(opts.outdir, 'tracks')
    try:
        os.makedirs(tracksdir)
    except OSError:
        pass

    # Go, go, go!
    logger.info('Starting PSO now')
    tStart = time.time()
    if opts.hpc_mode:
        os.chdir(os.path.join(opts.outdir, '../../optim/'))
    xopt, fopt = pso.pso(f, space['lb'], space['ub'], args=args, swarmsize=ss,
                         maxiter=opts.max_iterations, processes=procs,
                         dumpfile_prefix=os.path.join(tracksdir, 'track_%03d'),csv_output_path=opts.csv_output)
    tEnd = time.time()

    global count
    #logger.info('Number of iterations = %d', count)
    logger.info('xopt = %r', xopt)
    logger.info('fopt = %r', fopt)
    logger.info('PSO finished in %.3f [s]', tEnd - tStart)
    logger.info('Checking for SMF, BHBM and BHMF dump files...')
    dump_files = glob.glob(os.path.join(opts.outdir, 'SMF_z*_dump.txt'))
    logger.info('Found SMF dump files: %s', dump_files)
    dump_files2 = glob.glob(os.path.join(opts.outdir, 'BHMF_z*_dump.txt'))
    logger.info('Found BHMF dump files: %s', dump_files2)
    dump_files3 = glob.glob(os.path.join(opts.outdir, 'BHBM_z*_dump.txt'))
    logger.info('Found BHBM dump files: %s', dump_files3)

    logger.info('Running diagnostics...')
    diagnostics2.main(
        tracks_dir=os.path.join(opts.outdir, 'tracks'),
        space_file=opts.space_file, 
        output_dir=opts.outdir,
        config_opts=opts
    )

    # Clean up all files
    cleanup_files(opts)
    
if __name__ == '__main__':
    main()

