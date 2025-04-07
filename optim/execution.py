#!/bin/bash

import logging
import multiprocessing
import os
import shutil
import subprocess
import time

import numpy as np # type: ignore

import common


logger = logging.getLogger(__name__)

def count_jobs(job_name, username=None):
    """
    Returns how many jobs are currently queued or running.
    If username is provided, checks all jobs for that user.
    Otherwise, checks for jobs matching job_name.
    """
    try:
        if username:
            # Check jobs for specific user
            out, err, code = common.exec_command(["squeue", "-u", username])
        else:
            # Fall back to checking all jobs and filtering by name
            out, err, code = common.exec_command("squeue")
            
        if code:
            raise RuntimeError(f"squeue failed with code {code}: stdout: {out}, stderr: {err}")
        
        # Convert bytes to string if necessary
        if isinstance(out, bytes):
            out = out.decode('utf-8')
            
        if username:
            # For username query, just count lines (minus header)
            return max(0, len(out.splitlines()) - 1)
        else:
            # For job name, filter lines containing the name
            lines_with_jobname = [l for l in out.splitlines() if job_name in l]
            return len(lines_with_jobname)
            
    except OSError:
        raise RuntimeError("Couldn't run squeue, is it installed?")

#This is the original function and is just fine
def _exec_sage(msg, cmdline):
    logger.info('%s with command line: %s', msg, subprocess.list2cmdline(cmdline))
    out, err, code = common.exec_command(cmdline)
    if code != 0:
        logger.error('Error while executing %s (exit code %d):\n' +
                     'stdout:\n%s\nstderr:\n%s', cmdline[0], code,
                     common.b2s(out), common.b2s(err))
        raise RuntimeError('%s error' % cmdline[0])

def _evaluate(constraint, stat_test, modeldir, subvols):
    y_obs, y_mod, err = constraint.get_data(modeldir, subvols)
    return stat_test(y_obs, y_mod, err)

count = 0
def run_sage_hpc(particles, *args):
    """Modified version for SAGE with parallel particle execution."""
    global count, opts
    opts, space, subvols, statTest = args

    # Create base output directory for all particles
    modeldir = os.path.join(opts.outdir, f'DS_output_{count}/')
    if not os.path.exists(modeldir):
        os.makedirs(modeldir)

    fx = np.zeros(len(particles))
    processes = []

    # Determine if we should use SLURM
    use_slurm = opts.cpus > 4

    if use_slurm:
        # Prepare for SLURM submission
        job_name = f'PSOSMF_{count}'
        
        for i, particle in enumerate(particles):
            # Create particle subdirectory
            particle_dir = os.path.join(modeldir, f"{i}/")
            os.makedirs(particle_dir, exist_ok=True)
            
            # Get JOBFS path from environment
            jobfs = os.environ.get('JOBFS', '/var/tmp')
            work_dir = os.path.join(jobfs, 'jobfs')
            
            # Create particle-specific parameter file
            slash = len(opts.config) - opts.config[::-1].find('/') - 1
            temp_filename = os.path.join(opts.outdir, f'{opts.config[slash+1:-4]}_{count}_{i}_temp.par')
            
            # Modify parameter file with JOBFS path
            with open(opts.config) as f:
                lines = f.readlines()
                
            with open(temp_filename, 'w') as s:
                Ndone = 0
                for l, line in enumerate(lines):
                    if line[:9] == 'OutputDir':
                        lines[l] = f'OutputDir              {work_dir}\n'
                    for p, name in enumerate(space['name']):
                        if line[:len(name)] == name:
                            lines[l] = f'{name}          {str(round(particle[p],5))}\n'
                            Ndone += 1
                    if Ndone == len(space['name']):
                        break
                s.writelines(lines)

            logger.info(f'Submitting SAGE SLURM job for: {temp_filename}')

            # Create SLURM batch script
            batch_script = os.path.join(opts.outdir, f'slurm_script_{count}_{i}.slurm')
            with open(batch_script, 'w') as f:
                f.write("#!/bin/bash\n")
                f.write(f"#SBATCH --job-name={job_name}_{i}\n")
                f.write("#SBATCH --output=/dev/null\n")
                f.write("#SBATCH --error=/dev/null\n\n")
                f.write(f"#SBATCH --ntasks={opts.cpus}\n")
                f.write(f"#SBATCH --mem-per-cpu={opts.memory}\n")
                f.write("#SBATCH --tmp=200GB\n")

                if opts.walltime:
                    f.write(f"#SBATCH --time={opts.walltime}\n")
                if opts.account:
                    f.write(f"#SBATCH --account={opts.account}\n")
                if opts.queue:
                    f.write(f"#SBATCH --partition={opts.queue}\n")
                
                # Create work directory in JOBFS
                f.write("\n# Setup working directory\n")
                f.write(f'mkdir -p {work_dir}\n')
                
                # Show initial JOBFS state
                f.write('\necho "Initial JOBFS status:"\n')
                f.write('df -h $JOBFS\n')
                
                f.write("\nml purge\n")
                f.write("ml restore basic\n\n")
                
                # Run SAGE
                f.write(f"echo 'Starting SAGE job with {opts.cpus} CPUs'\n")
                f.write(f"mpirun -np {opts.cpus} {opts.sage_binary} {temp_filename}\n")
                
                # Copy results back with error checking
                f.write("\necho 'Copying results to permanent storage...'\n")
                f.write(f'if [ "$(ls -A {work_dir})" ]; then\n')
                f.write(f'    cp -r {work_dir}/* {particle_dir}\n')
                f.write('    echo "Copy completed successfully"\n')
                f.write('else\n')
                f.write('    echo "Error: No files found in JOBFS working directory"\n')
                f.write('    exit 1\n')
                f.write('fi\n\n')

            # Submit job using the batch script
            cmdline = ['sbatch', batch_script]
            process = subprocess.Popen(cmdline, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            out, err = process.communicate()
            
            if process.returncode != 0:
                raise RuntimeError(f"SLURM submission failed: {err.decode()}")
                
            # Extract job ID from sbatch output
            jobid = out.decode().strip().split()[-1]
            processes.append((i, jobid, temp_filename, particle_dir, batch_script))

        # Wait for all SLURM jobs to complete
        logger.info('Waiting for all SLURM jobs to complete...')
        #while True:
            #if count_jobs(job_name) == 0:
                #break
            #time.sleep(40)
        while True:
            if count_jobs(job_name, opts.username) == 0:
                # Add delay to allow file system to settle
                time.sleep(2)  
                break
            time.sleep(10)

    else:
        # Use MPI implementation for small CPU requests
        for i, particle in enumerate(particles):
            # Create particle subdirectory
            particle_dir = os.path.join(modeldir, f"{i}/")
            os.makedirs(particle_dir, exist_ok=True)
            
            # Create particle-specific parameter file
            slash = len(opts.config) - opts.config[::-1].find('/') - 1
            temp_filename = os.path.join(opts.outdir, f'{opts.config[slash+1:-4]}_{count}_{i}_temp.par')
            
            # Modify parameter file
            with open(opts.config) as f:
                lines = f.readlines()
                
            with open(temp_filename, 'w') as s:
                Ndone = 0
                for l, line in enumerate(lines):
                    if line[:9] == 'OutputDir':
                        lines[l] = f'OutputDir              {particle_dir}\n'
                    for p, name in enumerate(space['name']):
                        if line[:len(name)] == name:
                            lines[l] = f'{name}          {str(round(particle[p],5))}\n'
                            Ndone += 1
                    if Ndone == len(space['name']):
                        break
                s.writelines(lines)

            logger.info(f'Running SAGE instance: {temp_filename}')

            # Launch SAGE with MPI for each particle
            cmdline = [
                'mpirun',
                '-np', str(opts.cpus),
                opts.sage_binary,
                temp_filename
            ]

            # Launch process and store handle
            process = subprocess.Popen(cmdline, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append((i, process, temp_filename, particle_dir))
            
        # Wait for all MPI processes to complete
        for i, process, temp_filename, particle_dir in processes:
            logger.info('Waiting for all SAGE instances to complete...')
            out, err = process.communicate()
            
            if process.returncode != 0:
                logger.error(f"SAGE instance {i} failed with return code {process.returncode}")
                logger.error(f"stdout: {out.decode()}")
                logger.error(f"stderr: {err.decode()}")
                raise RuntimeError(f"SAGE instance {i} failed")

    # Process results and clean up
    for item in processes:
        if use_slurm:
            try:
                i, jobid, temp_filename, particle_dir, batch_script = item
            except ValueError:
                # Handle case where we might not have batch_script
                i, jobid, temp_filename, particle_dir = item
                batch_script = None
        else:
            i, process, temp_filename, particle_dir = item
            
        # Process results with retries
        max_retries = 1
        retry_delay = 10
        success = False
        #logger.info("Processing everything, combining HDF5 files if needed, etc. etc. etc.")
        
        for retry in range(max_retries):
            try:
                total_score = sum(_evaluate(c, statTest, particle_dir, subvols) for c in opts.constraints)
                fx[i] = total_score
                success = True
                break
            except Exception as e:
                logger.warning(f"Attempt {retry+1}: Error evaluating particle: {e}")
                time.sleep(retry_delay)
                
        if not success:
            logger.warning(f"Failed to process outputs for particle {i}  - assigning penalty score")
            logger.warning("This usually means SAGE is unhappy, bad parameter combination")
            fx[i] = 1e10

        # Clean up parameter file and batch script
        try:
            os.remove(temp_filename)
            if use_slurm:
                os.remove(batch_script)  # Clean up the batch script
        except OSError:
            pass

    # Clean up output directory if not keeping
    if not opts.keep:
        shutil.rmtree(modeldir)

    logger.info('Particles %r evaluated to %r', particles, fx)
    count += 1
    return fx

# this is the 'func' that is passed into the pso routine in pso.py
# this doesn't require the new pso.py file, can just run on the original pyswarm function
def run_sage(particle, *args):
    
    # space is the thing containing the parameter values for the model
    opts, space, subvols, statTest = args
    
    # create/clear directory for temporary Dark Sage output
    spid = str(multiprocessing.current_process().pid)
#    modeldir = '/Users/adam/DarkSage/autocalibration/DS_output_'+spid+'/'
    modeldir = os.path.join(opts.outdir, 'DS_output_' + spid + '/')
    if not os.path.exists(modeldir): os.makedirs(modeldir)
    if os.path.isfile(modeldir+'model_z0.000_0'): subprocess.call(['rm', modeldir+'model*'])

    # copy template parameter file and edit accordingly
    slash = len(opts.config) - opts.config[::-1].find('/') - 1
#    temp_filename = '/Users/adam/DarkSage/autocalibration/' + opts.config[slash+1:-4] + '_' + spid + '_temp.par'
    temp_filename = os.path.join(opts.outdir, opts.config[slash+1:-4] + '_' + spid + '_temp.par')
    #
    f = open(opts.config).readlines()
    s = open(temp_filename, 'w')
    #
    Np = len(space['name'])
    Ndone = 0
    for l, line in enumerate(f):
        if line[:9] == 'OutputDir': f[l] = 'OutputDir              '+modeldir+'\n'
        for p in range(Np):
            if line[:len(space['name'][p])] == space['name'][p]: 
                f[l] = space['name'][p]+'          '+str(round(particle[p],5))+'\n'
                Ndone += 1
        if Ndone==Np: break
    #
    s.writelines(f)
    s.close()

    # currently assuming that serial in parallel here is the best
    print('Running SAGE instance', temp_filename)
#    cmdline = ['mpirun', '-np', '8', opts.sage_binary, temp_filename]
    cmdline = [opts.sage_binary, temp_filename]
    _exec_sage('Running SAGE instance', cmdline)

    total = 10**sum(np.log10(np.sum(_evaluate(c, statTest, modeldir, subvols))*c.weight) for c in opts.constraints)
    logger.info('Particle %r evaluated to %f', particle, total)

    shutil.rmtree(modeldir)
    return total
