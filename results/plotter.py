import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
data = pd.read_csv('benchmark.csv')
data.replace(inplace=True,to_replace={'t':{-1:np.nan}})

WORKERS = data.w.unique()
SPACES = data.s.unique()
CARS = data.n.unique()

dfs = {}
cts = {}
tmin = np.zeros(len(CARS))
tput = np.ones([len(SPACES), len(WORKERS)]) * -np.inf
for k, car in enumerate(CARS):
	ddf = data[data.n == car]
	df = data[data.n == car].pivot('w', 's', 't')
	X = df.columns.values
	Y = df.index.values
	Z = df.values
	x, y = np.meshgrid(X, Y)
	dfs[car] = dict(x=x, y=y, z=Z, X=X, Y=Y, Z=Z)
	ddf = ddf.dropna(axis=0, how='any')
	cts[car] = dict(x=ddf.w, y=ddf.s, z=ddf.t)
	tmin[k] = np.min(ddf.t)
	ys = np.in1d(SPACES, X)
	xs = np.in1d(WORKERS, Y)
	ind = np.outer(xs, ys)
	z = Z.copy()
	z[np.where(np.isnan(z))] = -np.inf
	np.place(tput, ind, np.maximum(
		np.extract(ind, tput).reshape(np.sum(xs), np.sum(ys)),
		car / z
	))

def plot_tput(plt=plt):
	from matplotlib.ticker import MaxNLocator
	x, y = cts[1]['x'], cts[1]['y'],
	z = tput.flatten()
	fig = plt.figure()
	ax = fig.gca(projection='3d')
	surf = ax.plot_trisurf(x, y, z, cmap=plt.cm.jet, alpha=0.7, linewidth=0)
	ax.xaxis.set_major_locator(MaxNLocator(5))
	ax.yaxis.set_major_locator(MaxNLocator(6))
	ax.zaxis.set_major_locator(MaxNLocator(5))
	fig.tight_layout()
	ax.set_xlabel(r'$w$', fontsize=16)
	ax.set_ylabel(r'$s$', fontsize=16)
	ax.set_zlabel(r'$\eta$', fontsize=16)
	ax.set_title(r'Throughput')
	ax.grid(False)
	cb = plt.colorbar(surf, ax=ax, orientation='horizontal', fraction=0.046, pad=0.04)
	cb.ax.set_title(r'$\eta$', fontsize=12)
	ax.view_init(20, 60)


def plot_mint(plt=plt):
	t = np.zeros(len(CARS))
	for k, car in enumerate(CARS):
		t[k] = np.min( cts[car]['z'] )
	plt.plot(CARS, t)
	plt.xlabel(r'$n$', fontsize=16)
	plt.ylabel(r'$t_{\mathrm{min}}/\mathrm{sec}$', fontsize=16)
	plt.title(r'Minimum Production Time')

def plot_contour(n, plt=plt):
	x, y, z = dfs[n]['x'], dfs[n]['y'], dfs[n]['z']
	plt.contourf(x, y, z, alpha=0.7, cmap=plt.cm.jet)
	plt.yticks([1, 5, 10, 20, 30, 40, 50, 100, 120])
	plt.xlabel(r'$w$', fontsize=16)
	plt.ylabel(r'$s$', fontsize=16)
	plt.title(r'$n=' + str(n) + r'$', fontsize=16)
	clb = plt.colorbar()
	clb.ax.set_title(r'$t$', fontsize=12)
	return plt

def plot_surface(n, plt=plt):
	from matplotlib.ticker import MaxNLocator
	x, y, z = cts[n]['x'], cts[n]['y'], tput.flatten()#cts[n]['z']
	print(x.shape, y.shape, z.shape)
	fig = plt.figure()
	ax = fig.gca(projection='3d')
	surf = ax.plot_trisurf(x, y, z, cmap=plt.cm.jet, alpha=0.7, linewidth=0)
	ax.xaxis.set_major_locator(MaxNLocator(5))
	ax.yaxis.set_major_locator(MaxNLocator(6))
	ax.zaxis.set_major_locator(MaxNLocator(5))
	fig.tight_layout()
	ax.set_xlabel(r'$w$', fontsize=16)
	ax.set_ylabel(r'$s$', fontsize=16)
	ax.set_zlabel(r'$t$', fontsize=16)
	ax.set_title(r'$n=' + str(n) + r'$')
	ax.grid(False)
	cb = plt.colorbar(surf, ax=ax, orientation='horizontal', fraction=0.046, pad=0.04)
	cb.ax.set_title(r'$t$', fontsize=12)
	ax.view_init(20, 60)

