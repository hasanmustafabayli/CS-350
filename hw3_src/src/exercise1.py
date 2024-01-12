import os

def determine_cpus_and_w():
    num_cpus = os.cpu_count()
    w = num_cpus - 2

    print(f"Number of CPUs available: {num_cpus}")
    print(f"Value of w (worker threads): {w}")

determine_cpus_and_w()