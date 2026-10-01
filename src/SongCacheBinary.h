#ifndef SONG_CACHE_BINARY_H
#define SONG_CACHE_BINARY_H

#include <string>

class Song;

/**
 * @brief Binary song cache.
 *
 * The file layout is a fixed-order serialization driven by the field list in
 * SongCacheBinary.cpp; the writer and reader expand the same lists so they
 * cannot drift apart.
 *
 * Read() finishes with a TidyUpData(true) pass so the loaded song is left in a
 * consistent state. On failure (truncated, corrupt or stale file) it returns
 * false and leaves the song in an unspecified state, so the caller must Reset()
 * it and fall back to parsing the source simfiles.
 */
namespace SongCacheBinary {

bool Write(const Song& song, const std::string& path);
bool Read(Song& song, const std::string& path);

}  // namespace SongCacheBinary

#endif
