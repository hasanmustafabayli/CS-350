import matplotlib.pyplot as plt
import numpy as np

# 1. Open the text file for reading
file_path = 'one.txt'  # Replace with the correct file name and path

try:
    with open(file_path, 'r') as file:
        # 2. Read the contents of the file
        file_contents = file.read()

        # 3. Split the contents into a list of timestamps
        timestamps = file_contents.split('\n')  # Change '\n' to the appropriate delimiter

        # Optional: Remove empty strings from the list
        timestamps = list(filter(None, timestamps))

        # Convert the timestamps to floats (assuming they are in seconds)
        timestamps = [float(timestamp) for timestamp in timestamps]

        # 4. Calculate the inter-arrival times
        inter_arrival_times = [timestamps[i] - timestamps[i - 1] for i in range(1, len(timestamps))]

        # 5. Number of time bins and bin width
        num_bins = int(max(inter_arrival_times) / 0.005) + 1
        bin_width = 0.005

        # Create histogram data
        hist, bin_edges = np.histogram(inter_arrival_times, bins=num_bins, range=(0, max(inter_arrival_times) + bin_width))

        # Normalize the histogram
        hist = hist / 999  # Normalize by 999

        # Plot the distribution
        plt.bar(bin_edges[:-1], hist, width=bin_width, align='edge')
        plt.xlabel('Inter-Arrival Time (seconds)')
        plt.ylabel('Normalized Count')
        plt.title('Distribution of Inter-Arrival Times')
        plt.grid(True)

        # Show the plot
        plt.show()

except FileNotFoundError:
    print(f"The file '{file_path}' does not exist.")
except Exception as e:
    print(f"An error occurred: {e}")
