# Adapted from JavaScript to Python
# JavaScript's version original author: He "Lonnie" Liu
# https://s3.linkmeup.ru/linkmeup/ftree/index.html
# https://github.com/h8liu/ftree-vis/tree/master

import sys
import matplotlib as mpl
import matplotlib.pyplot as plt
import argparse
import matplotlib.patches as patches 

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--depth", type=int, default=2)
  parser.add_argument("--width", type=int, default=8)
  args = parser.parse_args()

  draw_fat_tree(depth=args.depth, width=args.width)
  plt.show()

class Node:
  def __init__(self):
    self.id = 0
    self.pos = [0, 0]


class Link:
  def __init__(self):
    self.src = 0
    self.dst = 0


class FatTree:
  def __init__(self):
    self.nodes = []
    self.links = []

def build_fat_tree(depth: int, width: int):
  pass

def draw_fat_tree(depth: int, width: int):
  k = width / 2
  padg = 13
  padi = 12
  hline = 70
  hhost = 50

  podw = 8
  podh = 8
  hostr = 2

  def kexp(n):
    return pow(k, n)
  
  if kexp(depth - 1) > 1500 or depth <= 0 or k <= 0:
    return

  w = kexp(depth - 1) * padg + 200
  h = (2 * depth) * hline
  
  ax = plt.gca()
  s = max(h, w)
  ax.set_ylim(-h / 2, h / 2)
  ax.set_xlim(-w / 2, w / 2)
  plt.gca().set_aspect(1)

  linePositions = []

  def podPositions(d):
    ret = []

    ngroup = kexp(d)
    pergroup = kexp(depth - 1 - d)

    wgroup = pergroup * padg
    wgroups = wgroup * (ngroup - 1)
    offset = -wgroups / 2

    for i in range(int(ngroup)):
      wpods = pergroup * padi;
      goffset = wgroup * i - wpods / 2
      
      for j in range(int(pergroup)):
          ret.append(offset + goffset + padi * j)

    return ret

  for i in range(int(depth)):
    linePositions.append(podPositions(i))

  def drawPods(list, y):
    j = 0
    n = len(list)
    for j in range(len(list)):
      rect_width = podw
      rect_height = podh
      rect_x = list[j] - podw / 2
      rect_y = y - podh / 2
      rect = patches.Rectangle(
        (rect_x, rect_y), rect_width, rect_height
      )
      plt.gca().add_patch(rect)

  def drawHost(x, y, dy, dx):
    x1 = x
    y1 = y
    x2 = x + dx
    y2 = y + dy
    plt.plot([x1, x2], [y1, y2])
    
    cx = x + dx
    cy = y + dy
    r = hostr
    circle = plt.Circle(
      (cx, cy), 1
    )
    plt.gca().add_patch(circle)

  def drawHosts(list, y, direction):
    for i in range(len(list)):
      if (k == 1):
        drawHost(list[i], y, hhost * direction, 0)
      elif (k == 2):
        drawHost(list[i], y, hhost * direction, -2)
        drawHost(list[i], y, hhost * direction, +2)
      elif (k == 3):
        drawHost(list[i], y, hhost * direction, -4)
        drawHost(list[i], y, hhost * direction, 0)
        drawHost(list[i], y, hhost * direction, +4)
      else:
        drawHost(list[i], y, hhost * direction, -4)
        drawHost(list[i], y, hhost * direction, 0)
        drawHost(list[i], y, hhost * direction, +4)
        
  def linePods(d, list1, list2, y1, y2):
    nonlocal k

    pergroup = kexp(depth - 1 - d)
    ngroup = kexp(d)

    perbundle = pergroup / k
    
    for i in range(int(ngroup)):
      offset = pergroup * i
      for j in range(int(k)):
        boffset = perbundle * j
        for t in range(int(perbundle)):
          ichild = int(offset + boffset + t)
          for d in range(int(k)):
            ifather = int(offset + perbundle * d + t)
            plt.plot([list1[ifather], list2[ichild]], [y1, y2])
          
    

  for i in range(depth - 1):
    linePods(i, linePositions[i], linePositions[i + 1], i * hline, (i + 1) * hline)
    linePods(i, linePositions[i], linePositions[i + 1], -i * hline, -(i + 1) * hline)

  drawHosts(linePositions[depth - 1], (depth - 1) * hline, 1);
  drawHosts(linePositions[depth - 1], -(depth - 1) * hline, -1);

  for i in range(int(depth)):
    if (i == 0):
      drawPods(linePositions[0], 0)
    else:
      drawPods(linePositions[i], i * hline)
      drawPods(linePositions[i], -i * hline)
    

  w = width / 2
  d = depth
  
  line = w ** (d - 1)

  nhost = 2 * line * w;
  nswitch = (2 * d - 1) * line;
  ncable = (2 * d) * w * line;
  ntx = 2 * (2 * d) * w * line;
  nswtx = ntx - nhost;

  print(f"nhost = {nhost}")
  print(f"nswitch = {nswitch}")
  print(f"ncable = {ncable}")
  print(f"ntx = {ntx}")
  print(f"nswtx = {nswtx}")


if __name__ == "__main__":
  main()