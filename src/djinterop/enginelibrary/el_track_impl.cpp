/*
    This file is part of libdjinterop.

    libdjinterop is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libdjinterop is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with libdjinterop.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include <djinterop/djinterop.hpp>
#include <djinterop/enginelibrary/el_crate_impl.hpp>
#include <djinterop/enginelibrary/el_database_impl.hpp>
#include <djinterop/enginelibrary/el_track_impl.hpp>
#include <djinterop/enginelibrary/el_transaction_guard_impl.hpp>
#include <djinterop/enginelibrary/track_utils.hpp>
#include <djinterop/util.hpp>

namespace djinterop::enginelibrary
{
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

namespace
{
// TODO (mr-smidge): consider moving anon namespace below into track_utils.hpp
stdx::optional<system_clock::time_point> to_time_point(
    stdx::optional<int64_t> timestamp)
{
    stdx::optional<system_clock::time_point> result;
    if (timestamp)
    {
        result = system_clock::time_point{seconds(*timestamp)};
    }
    return result;
}

stdx::optional<int64_t> to_timestamp(
    stdx::optional<system_clock::time_point> time)
{
    stdx::optional<int64_t> result;
    if (time)
    {
        result = duration_cast<seconds>(time->time_since_epoch()).count();
    }
    return result;
}

/// Calculate the samples-per-entry in an overview waveform.
///
/// An overview waveform always has 1024 entries, and the number of samples
/// that each one represents must be calculated from the true sample count by
/// rounding the number of samples to the quantisation number first.
int64_t calculate_overview_waveform_samples_per_entry(
    int64_t sample_rate, int64_t sample_count)
{
    auto qn = util::waveform_quantisation_number(sample_rate);
    if (qn == 0)
    {
        return 0;
    }

    return ((sample_count / qn) * qn) / 1024;
}

const int64_t default_track_type = 1;

const int64_t default_is_external_track = 0;

const stdx::optional<std::string> default_uuid_of_external_database;

const stdx::optional<int64_t> default_id_track_in_external_database;

const int64_t no_album_art_id = 1;

const int64_t default_pdb_import_key = 0;

const stdx::optional<std::string> default_uri;

const int64_t default_is_beatgrid_locked = 0;

const int64_t default_is_rendered = 0;

const int64_t default_has_serato_values = 0;

const int64_t default_has_rekordbox_values = 0;

const int64_t default_has_traktor_values = 0;

struct length_field_data
{
    stdx::optional<int64_t> length;
    stdx::optional<int64_t> length_calculated;
    stdx::optional<std::string> length_mm_ss;
};

length_field_data to_length_fields(
    stdx::optional<std::chrono::milliseconds> duration,
    stdx::optional<sampling_info> sampling)
{
    stdx::optional<int64_t> length;
    stdx::optional<std::string> length_mm_ss;
    if (duration)
    {
        length = static_cast<int64_t>(duration->count() / 1000);

        // String metadata, type 10, is the duration encoded as "MM:SS".
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0');
        oss << (*length / 60);
        oss << ":";
        oss << std::setw(2) << std::setfill('0');
        oss << (*length % 60);
        length_mm_ss = oss.str();
    }

    // A zero sample rate is interpreted as no sample rate.
    if (sampling && sampling->sample_rate == 0)
    {
        sampling = stdx::nullopt;
    }

    stdx::optional<int64_t> length_calculated;
    if (sampling)
    {
        length_calculated = static_cast<int64_t>(
            sampling->sample_count / sampling->sample_rate);
    }

    return length_field_data{length, length_calculated, length_mm_ss};
}

struct bpm_field_data
{
    stdx::optional<int64_t> bpm;
    stdx::optional<double> bpm_analyzed;
};

bpm_field_data to_bpm_fields(
    stdx::optional<double> bpm, stdx::optional<sampling_info> sampling,
    const std::vector<beatgrid_marker>& beatgrid)
{
    stdx::optional<int64_t> rounded_bpm;
    if (bpm)
    {
        rounded_bpm = static_cast<int64_t>(*bpm);
    }

    stdx::optional<double> bpm_analyzed;
    if (sampling && beatgrid.size() >= 2)
    {
        auto marker_1 = beatgrid[0];
        auto marker_2 = beatgrid[1];
        if (marker_1.sample_offset != marker_2.sample_offset)
        {
            bpm_analyzed = sampling->sample_rate * 60 *
                           (marker_2.index - marker_1.index) /
                           (marker_2.sample_offset - marker_1.sample_offset);
        }
    }

    return bpm_field_data{rounded_bpm, bpm_analyzed};
}

struct timestamp_field_data
{
    stdx::optional<int64_t> last_played_at_ts;
    stdx::optional<int64_t> last_modified_at_ts;
    stdx::optional<int64_t> last_accessed_at_ts;
    stdx::optional<std::string> ever_played;
};

timestamp_field_data to_timestamp_fields(
    stdx::optional<std::chrono::system_clock::time_point> last_played_at,
    stdx::optional<std::chrono::system_clock::time_point> last_modified_at,
    stdx::optional<std::chrono::system_clock::time_point> last_accessed_at)
{
    auto last_played_at_ts = to_timestamp(last_played_at);
    auto last_modified_at_ts = to_timestamp(last_modified_at);

    stdx::optional<int64_t> last_accessed_at_ts;
    if (last_accessed_at)
    {
        // Field is always ceiled to the midnight at the end of the day the
        // track is played, it seems.  This is believed to be due to the
        // hardware players using the VFAT ACCDATE to populate the field, which
        // is truncated only to a date.
        //
        // TODO (haslersn): Shouldn't we just set the unceiled time? This would
        // leave the decision whether to ceil it to the library user. Also, it
        // would make `el_track_impl::last_accessed_at()` consistent with the
        // value that has been set using this method.
        auto timestamp = *to_timestamp(last_accessed_at);
        auto secs_per_day = 86400;
        timestamp += secs_per_day - 1;
        timestamp -= timestamp % secs_per_day;
        last_accessed_at_ts = timestamp;
    }

    stdx::optional<std::string> ever_played =
        last_played_at ? stdx::optional<std::string>{"1"} : stdx::nullopt;

    return timestamp_field_data{
        last_played_at_ts, last_modified_at_ts, last_accessed_at_ts,
        ever_played};
}

stdx::optional<int64_t> to_key_num(stdx::optional<musical_key> key)
{
    stdx::optional<int64_t> key_num;
    if (key)
    {
        key_num = static_cast<int64_t>(*key);
    }

    return key_num;
}

track_data to_track_data(
    stdx::optional<sampling_info> sampling,
    stdx::optional<double> average_loudness, stdx::optional<musical_key> key)
{
    return track_data{sampling, average_loudness, key};
}

quick_cues_data to_cues_data(
    const std::array<stdx::optional<hot_cue>, 8>& hot_cues,
    stdx::optional<double> adjusted_main_cue,
    stdx::optional<double> default_main_cue)
{
    return quick_cues_data{
        hot_cues, adjusted_main_cue.value_or(0), default_main_cue.value_or(0)};
}

beat_data to_beat_data(
    stdx::optional<sampling_info> sampling,
    const std::vector<beatgrid_marker>& default_beatgrid,
    const std::vector<beatgrid_marker>& adjusted_beatgrid)
{
    return beat_data{sampling, default_beatgrid, adjusted_beatgrid};
}

loops_data to_loops_data(const std::array<stdx::optional<loop>, 8>& loops)
{
    return loops_data{loops};
}

overview_waveform_data to_overview_waveform_data(
    stdx::optional<sampling_info> sampling,
    const std::vector<waveform_entry>& waveform)
{
    auto sample_count = sampling ? sampling->sample_count : 0;
    auto sample_rate = sampling ? sampling->sample_rate : 0;
    double samples_per_entry = calculate_overview_waveform_samples_per_entry(
        sample_rate, sample_count);

    std::vector<waveform_entry> overview_waveform;
    if (!waveform.empty())
    {
        // Calculate an overview waveform automatically.
        // Note that the overview waveform always has 1024 entries in it.
        overview_waveform.reserve(1024);
        for (int32_t i = 0; i < 1024; ++i)
        {
            auto entry = waveform[waveform.size() * (2 * i + 1) / 2048];
            overview_waveform.push_back(entry);
        }
    }

    return overview_waveform_data{samples_per_entry, overview_waveform};
}

high_res_waveform_data to_high_res_waveform_data(
    stdx::optional<sampling_info> sampling,
    const std::vector<waveform_entry>& waveform)
{
    auto sample_rate = sampling ? sampling->sample_rate : 0;

    // Make the assumption that the client has respected the required number
    // of samples per entry when constructing the waveform.
    double samples_per_entry = util::waveform_quantisation_number(sample_rate);

    // TODO (mr-smidge) Rework to avoid copying the waveform here.
    return high_res_waveform_data{samples_per_entry, waveform};
}

}  // namespace

el_track_impl::el_track_impl(std::shared_ptr<el_storage> storage, int64_t id) :
    track_impl{id}, storage_{std::move(storage)}
{
}

beat_data el_track_impl::get_beat_data()
{
    return storage_->get_performance_data_column<beat_data>(id(), "beatData");
}

void el_track_impl::set_beat_data(beat_data data)
{
    storage_->set_performance_data_column(id(), "beatData", data);
}

high_res_waveform_data el_track_impl::get_high_res_waveform_data()
{
    return storage_->get_performance_data_column<high_res_waveform_data>(
        id(), "highResolutionWaveFormData");
}

void el_track_impl::set_high_res_waveform_data(high_res_waveform_data data)
{
    storage_->set_performance_data_column(
        id(), "highResolutionWaveFormData", data);
}

loops_data el_track_impl::get_loops_data()
{
    return storage_->get_performance_data_column<loops_data>(id(), "loops");
}

void el_track_impl::set_loops_data(loops_data data)
{
    storage_->set_performance_data_column(id(), "loops", data);
}

overview_waveform_data el_track_impl::get_overview_waveform_data()
{
    return storage_->get_performance_data_column<overview_waveform_data>(
        id(), "overviewWaveFormData");
}

void el_track_impl::set_overview_waveform_data(overview_waveform_data data)
{
    // As the overview waveform does not store opacity, it is defaulted to 255
    // when read back.  If we also set it to 255 here, we can apply a check in
    // `set_perfdata` that a round-trip encode/decode gives the same data.
    for (auto&& entry : data.waveform)
    {
        entry.low.opacity = 255;
        entry.mid.opacity = 255;
        entry.high.opacity = 255;
    }
    storage_->set_performance_data_column(id(), "overviewWaveFormData", data);
}

quick_cues_data el_track_impl::get_quick_cues_data()
{
    return storage_->get_performance_data_column<quick_cues_data>(
        id(), "quickCues");
}

void el_track_impl::set_quick_cues_data(quick_cues_data data)
{
    storage_->set_performance_data_column(id(), "quickCues", data);
}

track_data el_track_impl::get_track_data()
{
    return storage_->get_performance_data_column<track_data>(id(), "trackData");
}

void el_track_impl::set_track_data(track_data data)
{
    storage_->set_performance_data_column(id(), "trackData", data);
}

track_snapshot el_track_impl::snapshot() const
{
    track_snapshot snapshot{id()};

    auto track_data = storage_->get_track(id());
    auto meta_data = storage_->get_all_meta_data(id());
    auto meta_data_integer = storage_->get_all_meta_data_integer(id());
    auto perf_data = storage_->get_performance_data(id());

    snapshot.sampling = perf_data.track_performance_data
                            ? perf_data.track_performance_data->sampling
                            : stdx::nullopt;

    if (perf_data.beats)
    {
        snapshot.adjusted_beatgrid =
            std::move(perf_data.beats->adjusted_beatgrid);
    }
    snapshot.adjusted_main_cue =
        perf_data.quick_cues
            ? stdx::make_optional(perf_data.quick_cues->adjusted_main_cue)
            : stdx::nullopt;
    snapshot.average_loudness =
        perf_data.track_performance_data
            ? perf_data.track_performance_data->average_loudness
            : stdx::nullopt;
    snapshot.bitrate = track_data.bitrate;
    snapshot.bpm =
        track_data.bpm_analyzed
            ? track_data.bpm_analyzed
            : track_data.bpm
                  ? stdx::make_optional(static_cast<double>(*track_data.bpm))
                  : stdx::nullopt;
    if (perf_data.beats)
    {
        snapshot.default_beatgrid =
            std::move(perf_data.beats->default_beatgrid);
    }
    snapshot.default_main_cue =
        perf_data.quick_cues
            ? stdx::make_optional(perf_data.quick_cues->default_main_cue)
            : stdx::nullopt;
    if (snapshot.sampling)
    {
        auto ms = 1000.0 * snapshot.sampling->sample_count /
                  snapshot.sampling->sample_rate;
        snapshot.duration =
            stdx::make_optional(milliseconds{static_cast<int64_t>(ms)});
    }
    else if (track_data.length)
    {
        auto ms = 1000 * *track_data.length;
        snapshot.duration = stdx::make_optional(milliseconds{ms});
    }
    snapshot.file_bytes = track_data.file_bytes;
    if (perf_data.quick_cues)
    {
        snapshot.hot_cues = std::move(perf_data.quick_cues->hot_cues);
    }
    snapshot.key = perf_data.track_performance_data
                       ? perf_data.track_performance_data->key
                       : stdx::nullopt;
    if (perf_data.loops)
    {
        snapshot.loops = std::move(perf_data.loops->loops);
    }
    snapshot.relative_path = std::move(track_data.relative_path);
    snapshot.track_number =
        track_data.play_order
            ? stdx::make_optional(static_cast<int32_t>(*track_data.play_order))
            : stdx::nullopt;
    if (perf_data.high_res_waveform)
    {
        snapshot.waveform = std::move(perf_data.high_res_waveform->waveform);
    }
    snapshot.year =
        track_data.year
            ? stdx::make_optional(static_cast<int32_t>(*track_data.year))
            : stdx::nullopt;

    for (auto&& row : meta_data)
    {
        switch (row.type)
        {
            case metadata_str_type::title: snapshot.title = row.value; break;
            case metadata_str_type::artist: snapshot.artist = row.value; break;
            case metadata_str_type::album: snapshot.album = row.value; break;
            case metadata_str_type::genre: snapshot.genre = row.value; break;
            case metadata_str_type::comment:
                snapshot.comment = row.value;
                break;
            case metadata_str_type::publisher:
                snapshot.publisher = row.value;
                break;
            case metadata_str_type::composer:
                snapshot.composer = row.value;
                break;
            default: break;
        }
    }

    for (auto&& row : meta_data_integer)
    {
        switch (row.type)
        {
            case metadata_int_type::last_played_ts:
                snapshot.last_played_at = to_time_point(row.value);
                break;
            case metadata_int_type::last_modified_ts:
                snapshot.last_modified_at = to_time_point(row.value);
                break;
            case metadata_int_type::last_accessed_ts:
                snapshot.last_accessed_at = to_time_point(row.value);
                break;
            case metadata_int_type::musical_key:
                if (!snapshot.key)
                {
                    snapshot.key = stdx::make_optional(
                        static_cast<musical_key>(row.value));
                }

                break;
            default: break;
        }
    }

    return snapshot;
}

void el_track_impl::update(const track_snapshot& snapshot)
{
    if (snapshot.id && *snapshot.id != id())
    {
        throw invalid_track_snapshot{
            "Snapshot pertains to a different track, and so it cannot be used "
            "to update this track"};
    }
    else if (!snapshot.relative_path)
    {
        throw invalid_track_snapshot{
            "Snapshot does not contain a populated `relative_path` field, "
            "which is required on any track"};
    }

    auto length_fields = to_length_fields(snapshot.duration, snapshot.sampling);
    auto bpm_fields = to_bpm_fields(
        snapshot.bpm, snapshot.sampling, snapshot.adjusted_beatgrid);
    auto filename = get_filename(*snapshot.relative_path);
    auto extension = get_file_extension(filename);
    auto track_number =
        snapshot.track_number
            ? stdx::make_optional(static_cast<int64_t>(*snapshot.track_number))
            : stdx::nullopt;
    auto year = snapshot.year
                    ? stdx::make_optional(static_cast<int64_t>(*snapshot.year))
                    : stdx::nullopt;
    auto timestamp_fields = to_timestamp_fields(
        snapshot.last_played_at, snapshot.last_modified_at,
        snapshot.last_accessed_at);
    auto key_num = to_key_num(snapshot.key);
    auto clamped_rating = snapshot.rating
                              ? stdx::make_optional(static_cast<int64_t>(
                                    std::clamp(*snapshot.rating, 0, 100)))
                              : stdx::nullopt;
    stdx::optional<int64_t> last_play_hash;
    auto track_data = to_track_data(
        snapshot.sampling, snapshot.average_loudness, snapshot.key);
    auto overview_waveform_data =
        to_overview_waveform_data(snapshot.sampling, snapshot.waveform);
    auto high_res_waveform_data =
        to_high_res_waveform_data(snapshot.sampling, snapshot.waveform);
    auto beat_data = to_beat_data(
        snapshot.sampling, snapshot.default_beatgrid,
        snapshot.adjusted_beatgrid);
    auto cues_data = to_cues_data(
        snapshot.hot_cues, snapshot.adjusted_main_cue,
        snapshot.default_main_cue);
    auto loops_data = to_loops_data(snapshot.loops);

    el_transaction_guard_impl trans{storage_};

    // Firstly, update the `Track` table entry.
    storage_->update_track(
        id(), track_number, length_fields.length,
        length_fields.length_calculated, bpm_fields.bpm, year,
        snapshot.relative_path, filename, snapshot.bitrate,
        bpm_fields.bpm_analyzed, default_track_type, default_is_external_track,
        default_uuid_of_external_database,
        default_id_track_in_external_database, no_album_art_id,
        snapshot.file_bytes, default_pdb_import_key, default_uri,
        default_is_beatgrid_locked);

    // Set string-based metadata.
    storage_->set_meta_data(
        id(), snapshot.title, snapshot.artist, snapshot.album, snapshot.genre,
        snapshot.comment, snapshot.publisher, snapshot.composer,
        length_fields.length_mm_ss, timestamp_fields.ever_played, extension);

    // Set integer-based metadata.
    storage_->set_meta_data_integer(
        id(), key_num, clamped_rating, timestamp_fields.last_played_at_ts,
        timestamp_fields.last_modified_at_ts,
        timestamp_fields.last_accessed_at_ts, last_play_hash);

    // Set performance data, or remove, as appropriate.
    auto any_hot_cues = std::any_of(
        snapshot.hot_cues.begin(), snapshot.hot_cues.end(),
        [](auto hc) { return hc; });
    auto any_loops = std::any_of(
        snapshot.loops.begin(), snapshot.loops.end(), [](auto l) { return l; });
    auto has_perf_data = snapshot.sampling || snapshot.average_loudness ||
                         !snapshot.adjusted_beatgrid.empty() ||
                         !snapshot.default_beatgrid.empty() || any_hot_cues ||
                         any_loops;
    auto is_analysed = 1;
    if (has_perf_data)
    {
        storage_->set_performance_data(
            id(), is_analysed, default_is_rendered, track_data,
            high_res_waveform_data, overview_waveform_data, beat_data,
            cues_data, loops_data, default_has_serato_values,
            default_has_rekordbox_values, default_has_traktor_values);
    }
    else
    {
        storage_->clear_performance_data(id());
    }

    trans.commit();
}

std::vector<beatgrid_marker> el_track_impl::adjusted_beatgrid()
{
    auto beat_d = get_beat_data();
    return std::move(beat_d.adjusted_beatgrid);
}

void el_track_impl::set_adjusted_beatgrid(std::vector<beatgrid_marker> beatgrid)
{
    el_transaction_guard_impl trans{storage_};
    auto beat_d = get_beat_data();
    beat_d.adjusted_beatgrid = std::move(beatgrid);
    set_beat_data(std::move(beat_d));
    trans.commit();
}

double el_track_impl::adjusted_main_cue()
{
    return get_quick_cues_data().adjusted_main_cue;
}

void el_track_impl::set_adjusted_main_cue(double sample_offset)
{
    el_transaction_guard_impl trans{storage_};
    auto quick_cues_d = get_quick_cues_data();
    quick_cues_d.adjusted_main_cue = sample_offset;
    set_quick_cues_data(std::move(quick_cues_d));
    trans.commit();
}

stdx::optional<std::string> el_track_impl::album()
{
    return storage_->get_meta_data(id(), metadata_str_type::album);
}

void el_track_impl::set_album(stdx::optional<std::string> album)
{
    storage_->set_meta_data(id(), metadata_str_type::album, album);
}

stdx::optional<int64_t> el_track_impl::album_art_id()
{
    auto cell = storage_->get_track_column<int64_t>(id(), "idAlbumArt");
    stdx::optional<int64_t> album_art_id;
    if (cell < 1)
    {
        // TODO (haslersn): Throw something.
    }
    else if (cell > 1)
    {
        album_art_id = cell;
    }
    return album_art_id;
}

void el_track_impl::set_album_art_id(stdx::optional<int64_t> album_art_id)
{
    if (album_art_id && *album_art_id <= 1)
    {
        // TODO (haslersn): Throw something.
    }
    storage_->set_track_column(id(), "idAlbumArt", album_art_id.value_or(1));
    // 1 is the magic number for "no album art"
}

stdx::optional<std::string> el_track_impl::artist()
{
    return storage_->get_meta_data(id(), metadata_str_type::artist);
}

void el_track_impl::set_artist(stdx::optional<std::string> artist)
{
    return storage_->set_meta_data(id(), metadata_str_type::artist, artist);
}

stdx::optional<double> el_track_impl::average_loudness()
{
    return get_track_data().average_loudness;
}

void el_track_impl::set_average_loudness(
    stdx::optional<double> average_loudness)
{
    el_transaction_guard_impl trans{storage_};
    auto track_d = get_track_data();

    // Zero average loudness is interpreted as no average loudness.
    track_d.average_loudness =
        average_loudness.value_or(0) == 0 ? stdx::nullopt : average_loudness;

    set_track_data(track_d);
    trans.commit();
}

stdx::optional<int64_t> el_track_impl::bitrate()
{
    return storage_->get_track_column<stdx::optional<int64_t> >(
        id(), "bitrate");
}

void el_track_impl::set_bitrate(stdx::optional<int64_t> bitrate)
{
    storage_->set_track_column(id(), "bitrate", bitrate);
}

stdx::optional<double> el_track_impl::bpm()
{
    return storage_->get_track_column<stdx::optional<double> >(
        id(), "bpmAnalyzed");
}

void el_track_impl::set_bpm(stdx::optional<double> bpm)
{
    storage_->set_track_column(id(), "bpmAnalyzed", bpm);
    stdx::optional<int64_t> ceiled_bpm;
    if (bpm)
    {
        ceiled_bpm = static_cast<int64_t>(std::ceil(*bpm));
    }
    storage_->set_track_column(id(), "bpm", ceiled_bpm);
}

stdx::optional<std::string> el_track_impl::comment()
{
    return storage_->get_meta_data(id(), metadata_str_type::comment);
}

void el_track_impl::set_comment(stdx::optional<std::string> comment)
{
    storage_->set_meta_data(id(), metadata_str_type::comment, comment);
}

stdx::optional<std::string> el_track_impl::composer()
{
    return storage_->get_meta_data(id(), metadata_str_type::composer);
}

void el_track_impl::set_composer(stdx::optional<std::string> composer)
{
    storage_->set_meta_data(id(), metadata_str_type::composer, composer);
}

database el_track_impl::db()
{
    return database{std::make_shared<el_database_impl>(storage_)};
}

std::vector<crate> el_track_impl::containing_crates()
{
    std::vector<crate> results;
    storage_->db << "SELECT crateId FROM CrateTrackList WHERE trackId = ?"
                 << id() >>
        [&](int64_t id) {
            results.push_back(
                crate{std::make_shared<el_crate_impl>(storage_, id)});
        };
    return results;
}

std::vector<beatgrid_marker> el_track_impl::default_beatgrid()
{
    auto beat_d = get_beat_data();
    return std::move(beat_d.default_beatgrid);
}

void el_track_impl::set_default_beatgrid(std::vector<beatgrid_marker> beatgrid)
{
    el_transaction_guard_impl trans{storage_};
    auto beat_d = get_beat_data();
    beat_d.default_beatgrid = std::move(beatgrid);
    set_beat_data(std::move(beat_d));
    trans.commit();
}

double el_track_impl::default_main_cue()
{
    return get_quick_cues_data().default_main_cue;
}

void el_track_impl::set_default_main_cue(double sample_offset)
{
    el_transaction_guard_impl trans{storage_};
    auto quick_cues_d = get_quick_cues_data();
    quick_cues_d.default_main_cue = sample_offset;
    set_quick_cues_data(std::move(quick_cues_d));
    trans.commit();
}

stdx::optional<milliseconds> el_track_impl::duration()
{
    auto smp = sampling();
    if (smp)
    {
        double secs = smp->sample_count / smp->sample_rate;
        return milliseconds{static_cast<int64_t>(1000 * secs)};
    }
    auto secs =
        storage_->get_track_column<stdx::optional<int64_t> >(id(), "length");
    if (secs)
    {
        return milliseconds{*secs * 1000};
    }
    return stdx::nullopt;
}

std::string el_track_impl::file_extension()
{
    auto rel_path = relative_path();
    return get_file_extension(rel_path).value_or(std::string{});
}

std::string el_track_impl::filename()
{
    auto rel_path = relative_path();
    return get_filename(rel_path);
}

stdx::optional<std::string> el_track_impl::genre()
{
    return storage_->get_meta_data(id(), metadata_str_type::genre);
}

void el_track_impl::set_genre(stdx::optional<std::string> genre)
{
    storage_->set_meta_data(id(), metadata_str_type::genre, genre);
}

stdx::optional<hot_cue> el_track_impl::hot_cue_at(int32_t index)
{
    auto quick_cues_d = get_quick_cues_data();
    return std::move(quick_cues_d.hot_cues[index]);
}

void el_track_impl::set_hot_cue_at(int32_t index, stdx::optional<hot_cue> cue)
{
    el_transaction_guard_impl trans{storage_};
    auto quick_cues_d = get_quick_cues_data();
    quick_cues_d.hot_cues[index] = std::move(cue);
    set_quick_cues_data(std::move(quick_cues_d));
    trans.commit();
}

std::array<stdx::optional<hot_cue>, 8> el_track_impl::hot_cues()
{
    auto quick_cues_d = get_quick_cues_data();
    return std::move(quick_cues_d.hot_cues);
}

void el_track_impl::set_hot_cues(std::array<stdx::optional<hot_cue>, 8> cues)
{
    el_transaction_guard_impl trans{storage_};
    // TODO (haslersn): The following can be optimized because in this case we
    // overwrite all hot_cues
    auto quick_cues_d = get_quick_cues_data();
    quick_cues_d.hot_cues = std::move(cues);
    set_quick_cues_data(std::move(quick_cues_d));
    trans.commit();
}

stdx::optional<track_import_info> el_track_impl::import_info()
{
    if (storage_->get_track_column<int64_t>(id(), "isExternalTrack") == 0)
    {
        return stdx::nullopt;
    }
    return track_import_info{
        storage_->get_track_column<std::string>(id(), "uuidOfExternalDatabase"),
        storage_->get_track_column<int64_t>(id(), "idTrackInExternalDatabase")};
    // TODO (haslersn): How should we handle cells that unexpectedly don't
    // contain integral values?
}

void el_track_impl::set_import_info(
    const stdx::optional<track_import_info>& import_info)
{
    if (import_info)
    {
        storage_->set_track_column(id(), "isExternalTrack", 1);
        storage_->set_track_column(
            id(), "uuidOfExternalDatabase", import_info->external_db_uuid);
        storage_->set_track_column(
            id(), "idTrackInExternalDatabase", import_info->external_track_id);
    }
    else
    {
        storage_->set_track_column(id(), "isExternalTrack", 0);
        storage_->set_track_column(id(), "uuidOfExternalDatabase", nullptr);
        storage_->set_track_column(id(), "idTrackInExternalDatabase", nullptr);
    }
}

bool el_track_impl::is_valid()
{
    bool valid = false;
    storage_->db << "SELECT COUNT(*) FROM Track WHERE id = ?" << id() >>
        [&](int count) {
            if (count == 1)
            {
                valid = true;
            }
            else if (count > 1)
            {
                throw track_database_inconsistency{
                    "More than one track with the same ID", id()};
            }
        };
    return valid;
}

stdx::optional<musical_key> el_track_impl::key()
{
    stdx::optional<musical_key> result;
    auto key_num =
        storage_->get_meta_data_integer(id(), metadata_int_type::musical_key);
    if (key_num)
    {
        result = static_cast<musical_key>(*key_num);
    }
    return result;
}

void el_track_impl::set_key(stdx::optional<musical_key> key)
{
    stdx::optional<int64_t> key_num;
    if (key)
    {
        key_num = static_cast<int64_t>(*key);
    }

    el_transaction_guard_impl trans{storage_};
    auto track_d = get_track_data();
    track_d.key = key;
    set_track_data(track_d);
    storage_->set_meta_data_integer(
        id(), metadata_int_type::musical_key, key_num);
    trans.commit();
}

stdx::optional<system_clock::time_point> el_track_impl::last_accessed_at()

{
    // TODO (haslersn): Is there a difference between
    // `el_track_impl::last_accessed_at()` and
    // `el_track_impl::last_played_at()`, except for the ceiling of the
    // timestamp?
    return to_time_point(storage_->get_meta_data_integer(
        id(), metadata_int_type::last_accessed_ts));
}

void el_track_impl::set_last_accessed_at(
    stdx::optional<system_clock::time_point> accessed_at)
{
    if (accessed_at)
    {
        // Field is always ceiled to the midnight at the end of the day the
        // track is played, it seems.
        // TODO (haslersn): ^ Why "played" and not "accessed"?
        // TODO (haslersn): Shouldn't we just set the unceiled time? This would
        // leave the decision whether to ceil it to the library user. Also, it
        // would make `el_track_impl::last_accessed_at()` consistent with the
        // value that has been set using this method.
        auto timestamp = *to_timestamp(accessed_at);
        auto secs_per_day = 86400;
        timestamp += secs_per_day - 1;
        timestamp -= timestamp % secs_per_day;
        storage_->set_meta_data_integer(
            id(), metadata_int_type::last_accessed_ts, timestamp);
    }
    else
    {
        storage_->set_meta_data_integer(
            id(), metadata_int_type::last_accessed_ts, stdx::nullopt);
    }
}

stdx::optional<system_clock::time_point> el_track_impl::last_modified_at()
{
    return to_time_point(storage_->get_meta_data_integer(
        id(), metadata_int_type::last_modified_ts));
}

void el_track_impl::set_last_modified_at(
    stdx::optional<system_clock::time_point> modified_at)
{
    storage_->set_meta_data_integer(
        id(), metadata_int_type::last_modified_ts, to_timestamp(modified_at));
}

stdx::optional<system_clock::time_point> el_track_impl::last_played_at()
{
    return to_time_point(storage_->get_meta_data_integer(
        id(), metadata_int_type::last_played_ts));
}

void el_track_impl::set_last_played_at(
    stdx::optional<system_clock::time_point> played_at)
{
    static stdx::optional<std::string> zero{"0"};
    static stdx::optional<std::string> one{"1"};
    storage_->set_meta_data(
        id(), metadata_str_type::ever_played, played_at ? one : zero);
    storage_->set_meta_data_integer(
        id(), metadata_int_type::last_played_ts, to_timestamp(played_at));
    if (played_at)
    {
        // TODO (haslersn): Add entry to HistorylistTrackList
    }
    else
    {
        // TODO (haslersn): Should HistorylistTrackList now be cleared of this
        // track?
    }
}

stdx::optional<loop> el_track_impl::loop_at(int32_t index)
{
    auto loops_d = get_loops_data();
    return std::move(loops_d.loops[index]);
}

void el_track_impl::set_loop_at(int32_t index, stdx::optional<loop> l)
{
    el_transaction_guard_impl trans{storage_};
    auto loops_d = get_loops_data();
    loops_d.loops[index] = std::move(l);
    set_loops_data(std::move(loops_d));
    trans.commit();
}

std::array<stdx::optional<loop>, 8> el_track_impl::loops()
{
    auto loops_d = get_loops_data();
    return std::move(loops_d.loops);
}

void el_track_impl::set_loops(std::array<stdx::optional<loop>, 8> cues)
{
    el_transaction_guard_impl trans{storage_};
    loops_data loops_d;
    loops_d.loops = std::move(cues);
    set_loops_data(std::move(loops_d));
    trans.commit();
}

std::vector<waveform_entry> el_track_impl::overview_waveform()
{
    auto overview_waveform_d = get_overview_waveform_data();
    return std::move(overview_waveform_d.waveform);
}

stdx::optional<std::string> el_track_impl::publisher()
{
    return storage_->get_meta_data(id(), metadata_str_type::publisher);
}

void el_track_impl::set_publisher(stdx::optional<std::string> publisher)
{
    storage_->set_meta_data(id(), metadata_str_type::publisher, publisher);
}

stdx::optional<int32_t> el_track_impl::rating()
{
    auto result =
        storage_->get_meta_data_integer(id(), metadata_int_type::rating);
    return result ? stdx::make_optional(static_cast<int32_t>(*result))
                  : stdx::nullopt;
}

void el_track_impl::set_rating(stdx::optional<int32_t> rating)
{
    auto clamped_rating = rating ? stdx::make_optional(static_cast<int64_t>(
                                       std::clamp(*rating, 0, 100)))
                                 : stdx::nullopt;
    storage_->set_meta_data_integer(
        id(), metadata_int_type::rating, clamped_rating);
}

std::string el_track_impl::relative_path()
{
    return storage_->get_track_column<std::string>(id(), "path");
}

void el_track_impl::set_relative_path(std::string relative_path)
{
    // TODO (haslersn): Should we check the path?
    storage_->set_track_column(id(), "path", std::string{relative_path});
    auto filename = get_filename(relative_path);
    storage_->set_track_column(id(), "filename", std::string{filename});
    auto extension = get_file_extension(filename);
    storage_->set_meta_data(id(), metadata_str_type::file_extension, extension);
}

stdx::optional<sampling_info> el_track_impl::sampling()
{
    return get_track_data().sampling;
}

void el_track_impl::set_sampling(stdx::optional<sampling_info> sampling)
{
    el_transaction_guard_impl trans{storage_};

    // A zero sample rate is interpreted as no sample rate.
    if (sampling && sampling->sample_rate == 0)
    {
        sampling = stdx::nullopt;
    }

    stdx::optional<int64_t> secs;
    if (sampling && sampling->sample_rate != 0)
    {
        secs = static_cast<int64_t>(
            sampling->sample_count / sampling->sample_rate);

        // Set metadata duration_mm_ss as "MM:SS"
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0');
        oss << (*secs / 60);
        oss << ":";
        oss << (*secs % 60);
        auto str = oss.str();
        storage_->set_meta_data(
            id(), metadata_str_type::duration_mm_ss, std::string{str});
    }
    else
    {
        storage_->set_meta_data(
            id(), metadata_str_type::duration_mm_ss, stdx::nullopt);
    }
    storage_->set_track_column(id(), "length", secs);
    storage_->set_track_column(id(), "lengthCalculated", secs);

    // read old data
    auto track_d = get_track_data();
    auto beat_d = get_beat_data();
    auto high_res_waveform_d = get_high_res_waveform_data();
    auto overview_waveform_d = get_overview_waveform_data();

    // write new data
    track_d.sampling = sampling;
    beat_d.sampling = sampling;
    set_beat_data(std::move(beat_d));
    set_track_data(std::move(track_d));

    int64_t sample_rate = sampling ? sampling->sample_rate : 0;
    int64_t sample_count = sampling ? sampling->sample_count : 0;

    if (!high_res_waveform_d.waveform.empty())
    {
        // The high-resolution waveform has a required number of samples per
        // entry that is dependent on the sample rate.  If the sample rate is
        // genuinely changed using this method, note that the waveform is likely
        // to need to be updated as well afterwards.
        high_res_waveform_d.samples_per_entry =
            util::waveform_quantisation_number(sample_rate);
        set_high_res_waveform_data(std::move(high_res_waveform_d));
    }

    if (!overview_waveform_d.waveform.empty())
    {
        // The overview waveform has a varying number of samples per entry, as
        // the number of entries is always fixed.
        overview_waveform_d.samples_per_entry =
            calculate_overview_waveform_samples_per_entry(
                sample_rate, sample_count);
        set_overview_waveform_data(std::move(overview_waveform_d));
    }

    trans.commit();
}

stdx::optional<std::string> el_track_impl::title()
{
    return storage_->get_meta_data(id(), metadata_str_type::title);
}

void el_track_impl::set_title(stdx::optional<std::string> title)
{
    storage_->set_meta_data(id(), metadata_str_type::title, title);
}

stdx::optional<int32_t> el_track_impl::track_number()
{
    return storage_->get_track_column<stdx::optional<int32_t> >(
        id(), "playOrder");
}

void el_track_impl::set_track_number(stdx::optional<int32_t> track_number)
{
    storage_->set_track_column(id(), "playOrder", track_number);
}

std::vector<waveform_entry> el_track_impl::waveform()
{
    auto high_res_waveform_d = get_high_res_waveform_data();
    return std::move(high_res_waveform_d.waveform);
}

void el_track_impl::set_waveform(std::vector<waveform_entry> waveform)
{
    el_transaction_guard_impl trans{storage_};

    overview_waveform_data overview_waveform_d;
    high_res_waveform_data high_res_waveform_d;

    if (!waveform.empty())
    {
        auto smp = sampling();
        int64_t sample_count = smp ? smp->sample_count : 0;
        int64_t sample_rate = smp ? smp->sample_rate : 0;

        // Calculate an overview waveform automatically.
        // Note that the overview waveform always has 1024 entries in it.
        overview_waveform_d.samples_per_entry =
            calculate_overview_waveform_samples_per_entry(
                sample_rate, sample_count);
        overview_waveform_d.waveform.reserve(1024);
        for (int32_t i = 0; i < 1024; ++i)
        {
            auto entry = waveform[waveform.size() * (2 * i + 1) / 2048];
            overview_waveform_d.waveform.push_back(entry);
        }

        // Make the assumption that the client has respected the required number
        // of samples per entry when constructing the waveform.
        high_res_waveform_d.samples_per_entry =
            util::waveform_quantisation_number(sample_rate);
        high_res_waveform_d.waveform = std::move(waveform);
    }

    set_overview_waveform_data(std::move(overview_waveform_d));
    set_high_res_waveform_data(std::move(high_res_waveform_d));

    trans.commit();
}

stdx::optional<int32_t> el_track_impl::year()
{
    return storage_->get_track_column<stdx::optional<int32_t> >(id(), "year");
}

void el_track_impl::set_year(stdx::optional<int32_t> year)
{
    storage_->set_track_column(id(), "year", year);
}

track create_track(
    std::shared_ptr<el_storage> storage, const track_snapshot& snapshot)
{
    if (snapshot.id)
    {
        throw invalid_track_snapshot{
            "Snapshot already pertains to a persisted track, and so it cannot "
            "be created again"};
    }
    else if (!snapshot.relative_path)
    {
        throw invalid_track_snapshot{
            "Snapshot does not contain a populated `relative_path` field, "
            "which is required to create a track"};
    }

    auto length_fields = to_length_fields(snapshot.duration, snapshot.sampling);
    auto bpm_fields = to_bpm_fields(
        snapshot.bpm, snapshot.sampling, snapshot.adjusted_beatgrid);
    auto filename = get_filename(*snapshot.relative_path);
    auto extension = get_file_extension(filename);
    auto track_number =
        snapshot.track_number
            ? stdx::make_optional(static_cast<int64_t>(*snapshot.track_number))
            : stdx::nullopt;
    auto year = snapshot.year
                    ? stdx::make_optional(static_cast<int64_t>(*snapshot.year))
                    : stdx::nullopt;
    auto timestamp_fields = to_timestamp_fields(
        snapshot.last_played_at, snapshot.last_modified_at,
        snapshot.last_accessed_at);
    auto key_num = to_key_num(snapshot.key);
    auto clamped_rating = snapshot.rating
                              ? stdx::make_optional(static_cast<int64_t>(
                                    std::clamp(*snapshot.rating, 0, 100)))
                              : stdx::nullopt;
    stdx::optional<int64_t> last_play_hash;
    auto track_data = to_track_data(
        snapshot.sampling, snapshot.average_loudness, snapshot.key);
    auto overview_waveform_data =
        to_overview_waveform_data(snapshot.sampling, snapshot.waveform);
    auto high_res_waveform_data =
        to_high_res_waveform_data(snapshot.sampling, snapshot.waveform);
    auto beat_data = to_beat_data(
        snapshot.sampling, snapshot.default_beatgrid,
        snapshot.adjusted_beatgrid);
    auto cues_data = to_cues_data(
        snapshot.hot_cues, snapshot.adjusted_main_cue,
        snapshot.default_main_cue);
    auto loops_data = to_loops_data(snapshot.loops);

    el_transaction_guard_impl trans{storage};

    // Firstly, create the `Track` table entry.
    auto id = storage->create_track(
        track_number, length_fields.length, length_fields.length_calculated,
        bpm_fields.bpm, year, snapshot.relative_path, filename,
        snapshot.bitrate, bpm_fields.bpm_analyzed, default_track_type,
        default_is_external_track, default_uuid_of_external_database,
        default_id_track_in_external_database, no_album_art_id,
        snapshot.file_bytes, default_pdb_import_key, default_uri,
        default_is_beatgrid_locked);

    // Set string-based metadata.
    storage->set_meta_data(
        id, snapshot.title, snapshot.artist, snapshot.album, snapshot.genre,
        snapshot.comment, snapshot.publisher, snapshot.composer,
        length_fields.length_mm_ss, timestamp_fields.ever_played, extension);

    // Set integer-based metadata.
    storage->set_meta_data_integer(
        id, key_num, clamped_rating, timestamp_fields.last_played_at_ts,
        timestamp_fields.last_modified_at_ts,
        timestamp_fields.last_accessed_at_ts, last_play_hash);

    // Set performance data, if any.
    auto any_hot_cues = std::any_of(
        snapshot.hot_cues.begin(), snapshot.hot_cues.end(),
        [](auto hc) { return hc; });
    auto any_loops = std::any_of(
        snapshot.loops.begin(), snapshot.loops.end(), [](auto l) { return l; });
    auto has_perf_data = snapshot.sampling || snapshot.average_loudness ||
                         !snapshot.adjusted_beatgrid.empty() ||
                         !snapshot.default_beatgrid.empty() || any_hot_cues ||
                         any_loops;
    auto is_analysed = 1;
    if (has_perf_data)
    {
        storage->set_performance_data(
            id, is_analysed, default_is_rendered, track_data,
            high_res_waveform_data, overview_waveform_data, beat_data,
            cues_data, loops_data, default_has_serato_values,
            default_has_rekordbox_values, default_has_traktor_values);
    }

    track tr{std::make_shared<el_track_impl>(storage, id)};

    trans.commit();

    return tr;
}

}  // namespace djinterop::enginelibrary
