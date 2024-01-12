import matplotlib.pyplot as plt
import numpy as np

# 1. Open the text file for reading
file_path = 'one.txt'  # Replace 'one.txt' with the correct file name and path

try:
    with open(file_path, 'r') as file:
        # 2. Read the contents of the file
        file_contents = file.read()

        # 3. Split the contents into a list based on a delimiter
        request_lengths = file_contents.split('\n')  # Change '\n' to the appropriate delimiter

        # Optional: Remove empty strings from the list
        request_lengths = list(filter(None, request_lengths))

        # Convert the request_lengths to floats
        request_lengths = [float(length) for length in request_lengths]

        # Now, request_lengths contains the data from "one.txt" as a list of floats

        # Number of time bins and bin width
        num_bins = int(max(request_lengths) / 0.005) + 1
        bin_width = 0.005

        # Create histogram data
        hist, bin_edges = np.histogram(request_lengths, bins=num_bins, range=(0, max(request_lengths) + bin_width))

        # Normalize the histogram
        hist = hist / 1000  # Assuming 1000 requests in total

        # Plot the distribution
        plt.bar(bin_edges[:-1], hist, width=bin_width, align='edge')
        plt.xlabel('Request Length (seconds)')
        plt.ylabel('Normalized Count')
        plt.title('Distribution of Request Lengths')
        plt.grid(True)

        # Show the plot
        plt.show()

except FileNotFoundError:
    print(f"The file '{file_path}' does not exist.")
except Exception as e:
    print(f"An error occurred: {e}")
