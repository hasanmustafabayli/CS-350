import matplotlib.pyplot as plt


def process_data_from_file(file_path):
    inter_arrival_times = []
    request_lengths = []
    with open(file_path, 'r') as file:
        lines = file.readlines()
        for line in lines:
            if line.startswith('R') or line.startswith('[#CLIENT#] R'):
                parts = line.split(':')
                if len(parts) == 2:
                    info = parts[1].split(',')
                    if len(info) >= 5:
                        inter_arrival_times.append(float(info[3]) - float(info[0]))
                        request_lengths.append(float(info[1]))
    return inter_arrival_times, request_lengths

file_path = "/Users/hasanmustafabayli/Desktop/CS350/hw3_src/src/data2.txt"  # Replace with the path to your file
inter_arrival_times, request_lengths = process_data_from_file(file_path)

# Print the extracted inter-arrival times and request lengths
print("Inter-arrival Times:", inter_arrival_times)
print("Request Lengths:", request_lengths)

# Plotting inter-arrival times
plt.figure(figsize=(10, 5))
plt.subplot(1, 2, 1)
plt.plot(range(len(inter_arrival_times)), inter_arrival_times, marker='o')
plt.title('Inter-arrival Times')
plt.xlabel('Request Number')
plt.ylabel('Inter-arrival Time')

# Plotting request lengths
plt.subplot(1, 2, 2)
plt.plot(range(len(request_lengths)), request_lengths, marker='o')
plt.title('Request Lengths')
plt.xlabel('Request Number')
plt.ylabel('Request Length')

plt.show()
