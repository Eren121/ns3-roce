import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
import project

class Model:
    def __init__(self):
        self.l = 0.1 # Loss proportion (in [0;1])
        self.e = 1 # FEC efficiency (in [0;1])
        self.b = 4096 # Chunk size (B)
        self.g = 100e9 / 8 # Bandwidth (Bytes per second)
        self.c0 = 100_000 # Data chunks per block
        self.set_n(4, 4) # Count of nodes
        self.set_k(2) # Count of multicast roots
        self.set_c1(0) # Parity chunks per block (no FEC initially)
        self.set_d0(1e-6)
    
    def set_d0(self, d0) -> None:
        self.d0 = d0 # Delay between servers of same leaf switches (s)
        self.d1 = 2 * self.d0 # Delay between servers of different leaf switches (s)
        self.dn = ((self.s - 1) * self.d0 + self.d1) / self.s
    
    def set_c1(self, c1) -> None:
        self.c1 = c1
        self.c = self.c1 + self.c0

    def set_n(self, s, m) -> None:
        self.s = s # Server per leaf switch
        self.m = m # Count of leaf switches
        self.n = self.s * self.m
    
    def set_k(self, k) -> None:
        self.k = k
        self.gprim = self.k / self.g
        self.nc = self.n / self.k

def plot_1() -> None:
    """
    Plot:
        X axis: Loss percentage
        Y axis: Percent of data chunks to send as additional parity chunks
    """

    # Chunk loss proportion
    l = np.linspace(0, 0.5, num=50)
    
    # Chunk size (B)
    b = 4096

    # bandwidth (bps)
    g = 100e9

    # Data chunks per block
    c0 = 100_000

    # Efficiency in [0;1]
    e = 1

    # Proportion of parity chunks over data chunks to additionaly send to receive 100% of data
    """
    r = ef * (1 - l) * c1
    r + (1 - l) * c0 = c0
    r = c0 - (1 - l) * c0
    r = c0 * l
    
    c0 * l = ef * (1 - l) * c1
    """
    c1 = c0 * l / (e * (1 - l))

    l_p = l * 100

    plt.plot(l_p, c1 / c0 * 100, label="data")
    plt.plot(l_p , l_p, label="x=y")
    plt.legend()
    ax = plt.gca()
    ax.set_xlabel("Chunk loss rate (%)")
    ax.set_ylabel("Addition parity chunks (% of data chunks)")
    ax.set_ylim(bottom=0)
    ax.set_xlim(left=0)
    plt.grid()
    plt.show()


    # Delays (s)
    d0 = 1e-6
    d1 = d0 * 2

    # Topology
    s = 4
    m = 4
    n = s * m

    # Assuming we send this ideal count of parity chunks for this given chunk loss rate,
    # Compute the cost

    # Allgather completion time w/ FEC
    fec_t = n * (d1 + b * (c0 + c1) / g)

    # Mcast completion time w/o FEC
    mc_t = n * (d1 + b * (c0) / g)

    # Lost data chunks if we don't apply FEC
    cm = l * c0 # - e * (1 - l) * c1

    # Recovery elapsed time w/o FEC
    rec_t = (d0 + cm * b / g) * m * (s - 1) + (d1 + cm * b / g) * m
    # Allgather completion w/o FEC
    t = mc_t + rec_t

    plt.plot(l_p, fec_t, label="FEC time")
    plt.plot(l_p, t, label="no FEC time")
    plt.legend()
    plt.grid()
    ax = plt.gca()
    ax.set_xlabel("Chunk loss rate (%)")
    ax.set_ylabel("Completion time (s)")
    ax.set_ylim(bottom=0)
    ax.set_xlim(left=0)
    plt.show()

    # Bandwidth usage
    bu_fec = b * n * (c0 + c1)
    bu_mc = b * n * (c0)
    bu_rec = 2 * cm * b * (n - 1)
    bu = bu_mc + bu_rec

    plt.plot(l_p, bu_fec, label="FEC bandwidth usage")
    plt.plot(l_p, bu, label="no FEC bandwidth usage")
    plt.legend()
    plt.grid()
    ax = plt.gca()
    ax.set_xlabel("Chunk loss rate (%)")
    ax.set_ylabel("Bandwidth usage (B)")
    ax.set_ylim(bottom=0)
    ax.set_xlim(left=0)
    plt.show()


def plot_2(e: float) -> None:
    """
    Plot:
        X Axis: % of additional parity chunks (c1/c0 * 100)
        Y Axis: Arbitrary normalized Y axis, we plot multiple data
            1) % of lost data chunks in the multicast phase
            2) % of time cost relatively to no FEC
            3) % of bandwidth cost relatively to no FEC
    """

    # Variables that does not depends on any other
    k = 2
    s = 256
    m = 16
    d0 = 1e-6
    b = 1_000
    g = 100e9 / 8
    l = 0.2
    c0 = 1_000_000

    c0s = np.linspace(0, 0.75, 500) * c0
    xs = np.zeros(len(c0s))
    ys_loss = np.zeros(len(c0s))
    ys_t = np.zeros(len(c0s))
    ys_u = np.zeros(len(c0s))

    for i, c1 in enumerate(c0s):
        c = c0 + c1
        n = s * m
        d1 = 2 * d0
        nc = n/k

        cm = max(0, l*c0 - e*(1-l)*c1)
        dn = ((s-1)*d0+d1)/s
        tm = n*b*c/g + (nc - 1)*dn + d1
        tr = (b*cm/(g*(1-l)) + dn) * (n - 1)
        t = tm + tr
        um = b*c*n
        ur = 2*b/(1-l)*cm*(n-1)
        u = um + ur

        xs[i] = c1 / c0
        ys_loss[i] = cm / c0 # Ratio of lost data chunks in mcast phase
        ys_t[i] = t
        ys_u[i] = u
    
    # Make relative to w/o FEC
    ys_t /= ys_t[0]
    ys_u /= ys_u[0]
    
    # To Percent
    xs *= 100
    ys_loss *= 100
    ys_t *= 100
    ys_u *= 100

    plt.plot(xs, ys_loss, label="Data chunks loss % in multicast", linestyle="dotted", linewidth=2)
    plt.plot(xs, ys_t, label="Allgather completion time", linestyle='dashdot', linewidth=2)
    plt.plot(xs, ys_u, label="Allgather bandwidth usage", linewidth=2)
    plt.plot(xs, np.ones(len(c0s)) * 100, linewidth=1, label="Baseline")
    plt.legend(loc='center', bbox_to_anchor=(0,0,1,0.75), fancybox=True, shadow=True)
    #plt.title(f"$e$={e},$l$={l}")
    plt.title(f"Efficiency = {e}, Loss rate = {l}")
    plt.grid()
    ax = plt.gca()
    ax.set_xlabel("Additional parity chunks in % of data chunks")
    ax.set_ylabel("Percent")
    ax.set_ylim(bottom=0)
    ax.set_xlim(left=0)
    #plt.show()
    plt.savefig(project.analysis_path() / "plots" / f"efficiency_{e}.pdf")
    plt.clf()

def main() -> None:
    # plt.rcParams['text.usetex'] = True
    #plot_1()
    plot_2(0.5)
    plot_2(0.75)
    plot_2(0.4)

if __name__ == "__main__":
    main()