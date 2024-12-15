import matplotlib.pyplot as plt
import numpy as np

# 데이터
categories = ['8x8 partitioning', '4x4 partitioning']
client_client_time = [4.3, 3.0]
client_server_time = [2.5, 4.5]
server_io_time = [2.0, 2.0]

x = np.arange(len(categories))  # 카테고리의 인덱스 위치
width = 0.25  # 막대 너비

# 그래프 그리기
fig, ax = plt.subplots()
bar1 = ax.bar(x - width, client_client_time, width, label='client-client comm.', color='royalblue')
bar2 = ax.bar(x, client_server_time, width, label='client-server comm.', color='orangered')
bar3 = ax.bar(x + width, server_io_time, width, label='server I/O', color='gray')

# 그래프 설정
ax.set_xlabel('Partitioning')
ax.set_ylabel('Time (sec.)')
ax.set_title('Data Visualization')
ax.set_xticks(x)
ax.set_xticklabels(categories)
ax.legend()

plt.tight_layout()
plt.show()