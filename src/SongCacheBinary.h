#ifndef SONG_CACHE_BINARY_H
#define SONG_CACHE_BINARY_H

#include <string>

class Song;

/**
 * @brief Binary song cache.
 *
 * Replaces the old text (.ssc-shaped) song cache. The file layout is a
 * fixed-order serialization driven by the field list in SongCacheBinary.cpp;
 * the writer and reader expand the same lists so they cannot drift apart.
 *
 * The reader is a drop-in replacement for
 * SSCLoader::LoadFromSimfile(path, song, true): it leaves the song in the same
 * state, including the TidyUpData(false) pass. On failure (truncated, corrupt
 * or stale file) it returns false and leaves the song in an unspecified state,
 * so the caller must Reset() it and fall back to parsing the source simfiles.
 */
namespace SongCacheBinary {

bool Write(const Song& song, const std::string& path);
bool Read(Song& song, const std::string& path);

}  // namespace SongCacheBinary

#endif
