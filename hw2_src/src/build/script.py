import pandas as pd
import numpy as np
import sys
import matplotlib.pyplot as plt
file_num=sys.argv[1]
csv_data=pd.read_csv(f"qvals/qvals{file_num}.csv",engine="python",sep="[,:]")
queues=open(f"queues/qupdate{file_num}.txt","r")
queues_num=[]
for i in range(1000):
    queues_num.append(queues.readline().count("R"))
avg_q=0
total_time=csv_data.iloc[-1,5]-csv_data.iloc[0,3]
for i in range(999):
    queue_time=csv_data.iloc[i+1,3]-csv_data.iloc[i,4]
    avg_q+=queues_num[i]*(queue_time/total_time)
csv_data['response time']=csv_data['completed']-csv_data['timestamp']
avg_response_time=csv_data['response time'].mean()
server_util=csv_data['length'].sum()/(csv_data['completed'].iloc[-1]-csv_data['recieved'][0])
print(avg_q)
print(avg_response_time)
print(server_util)