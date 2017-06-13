Description
===========

Reads a mpls file and returns a dictionary, not a clip.

There are three elements in the dictionary:
* key 'clip' contains a list of full paths to each m2ts file in the playlist.
* key 'filename' contains a list of filenames of each m2ts file in the playlist.
* key 'count' contains the number of m2ts files in the playlist.


Usage
=====

    mpls.Read(string source[, int angle=0])

* source: The full path of the mpls file. Don't use relative path.

* angle: The angle index to select in the playlist. Index numbers start from zero. If the playlist isn't multi-angle, this setting does nothing.


After obtaining the dictionary, you can use your favorite source filter to open them all with a for-loop and splice them together. For example:

```python
mpls = core.mpls.Read('D:/rule6/BDMV/PLAYLIST/00001.mpls')
ret = core.std.Splice([core.ffms2.Source(mpls['clip'][i]) for i in range(mpls['count'])])
```

If you want to put ffms2's index file at another place rather than the same directory of the source file, here is the example:

```python
mpls = core.mpls.Read('D:/rule6/BDMV/PLAYLIST/00001.mpls')
clips = []
for i in range(mpls['count']):
    clips.append(core.ffms2.Source(source=mpls['clip'][i], cachefile='D:/indexes/rule6/' + mpls['filename'][i].decode() + '.ffindex'))
clip = core.std.Splice(clips)
```


Compilation
===========

Requires libbluray for compiling.

```
./autogen.sh
./configure
make
```
