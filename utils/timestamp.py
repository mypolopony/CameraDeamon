import re
from numpy import diff, linspace
import plotly
import plotly.graph_objs as go

# Nanoseconds to frames per second
def nsec2fps(nsec):
    return 1./(float(nsec)/1000000000.)

# Microseconds frames per second
def usec2fps(usec):
    return 1./(float(usec)/1000000.)

'''
basename = 'aca1300-6000'
logfile = '/home/agridata/pylon-5.0.5.9000-x86_64/Samples/C++/Grab_MultipleCameras/{}.txt'.format(basename)
cameras = dict()

with open(logfile,'r') as log:
    for line in log:
        if 'Camera' in line and ':' in line:
            camera = re.search('acA.+',line).group(0)
            if camera not in cameras.keys():
                cameras[camera] = list()
        if 'Timestamp' in line:
            timestamp = int(re.search('[0-9]+',line).group(0))
            cameras[camera].append(timestamp)

traces = list()
for key in cameras.keys():
    cameras[key] = diff(cameras[key])
    cameras[key] = [nsec2fps(x) for x in cameras[key]]
    traces.append(go.Scatter(
            x = linspace(1, len(cameras[key]), len(cameras[key])),
            y = cameras[key],
            name = key,
            mode = 'lines'))
'''

from dateutil import parser
import pandas as pd

logfile='/home/agridata/output/d3cd6a96_2017-03-08T18:13:59.757956934-08:00.txt'
df = pd.read_csv(logfile)

traces=list()

oldts = diff([parser.parse(time) for time in df['Timestamp'].as_matrix()])
oldts = [usec2fps(time.microseconds) for time in oldts]
traces.append(go.Scatter(
    x = linspace(1, len(oldts), len(oldts)),
    y = oldts,
    name = 'Old Timestamps',
    mode = 'lines'))

newts = [nsec2fps(time) for time in diff(df['timestamp2'].as_matrix())]
traces.append(go.Scatter(
    x = linspace(1, len(newts), len(newts)),
    y = newts,
    name = 'New Timestamps',
    mode = 'lines'))

layout = go.Layout(
    title='Frame Rate Analysis',
    xaxis=dict(
        title='Frame Number',
        titlefont=dict(
            family='Courier New, monospace',
            size=18,
            color='#0c0c0c'
        )
    ),
    yaxis=dict(
        title='Frame rate (fps)',
        titlefont=dict(
            family='Courier New, monospace',
            size=18,
            color='#0c0c0c'
        )
    )
)

fig = go.Figure(data=traces, layout=layout)
plotly.offline.plot(fig, filename='comparison2.html')