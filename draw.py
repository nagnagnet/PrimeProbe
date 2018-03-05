import matplotlib.pyplot as plt

mylist = []
x = []
y = []
num = 0
THR = 200

f = open("data.txt", "r")
for line in f:
	tmp = line.split()
	line = list(map(int, tmp))
	mylist.append(line)
f.close()

for line in mylist:
	hit = 0
	for i in line:
		if i > THR:
			hit += 1
	#pickup 
	if hit > 30 and hit < 200:
		tmp = []
		for i in range(len(line)):
			if(line[i] > THR):
				tmp.append(i)
		x.append(tmp)
		tmp = []
		for i in range(hit):
			tmp.append(num)
		y.append(tmp)
	num += 1

for i in range(len(x)):
	plt.scatter(x[i], y[i], c = 'b')
plt.xlim([0,600])
plt.ylim([0,255])
plt.show()
