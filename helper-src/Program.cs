using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Realms;

class Program
{
    static readonly string[] Invalid = Path.GetInvalidFileNameChars().Select(c => c.ToString()).ToArray();

    static string TryStr(Func<object> f)
    {
        try
        {
            var v = f();
            return v?.ToString() ?? "";
        }
        catch { return ""; }
    }

    static void Main(string[] args)
    {
        if (args.Length < 3)
        {
            Console.WriteLine("usage: OszExporter <client.realm> <filesRoot> <outDir> [--since yyyy-MM-dd] [--dump]");
            return;
        }

        string realmPath = args[0];
        string filesRoot = args[1];
        string outDir = args[2];
        DateTimeOffset? since = null;
        bool dumpOnly = false;
        string tsvPath = null;
        string idsFile = null;

        for (int i = 3; i < args.Length; i++)
        {
            if (args[i] == "--since" && i + 1 < args.Length)
                since = DateTimeOffset.Parse(args[++i], CultureInfo.InvariantCulture, DateTimeStyles.AssumeLocal);
            else if (args[i] == "--tsv" && i + 1 < args.Length)
                tsvPath = args[++i];
            else if (args[i] == "--ids-file" && i + 1 < args.Length)
                idsFile = args[++i];
            else if (args[i] == "--dump")
                dumpOnly = true;
        }

        var config = new RealmConfiguration(realmPath) { IsDynamic = true };
        using var realm = Realm.GetInstance(config);

        var sets = new List<dynamic>();
        foreach (var raw in realm.DynamicApi.All("BeatmapSet").ToList())
            sets.Add(raw);

        sets.Sort((a, b) => ((DateTimeOffset)b.DateAdded).CompareTo((DateTimeOffset)a.DateAdded));

        if (tsvPath != null)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(tsvPath)));
            WriteTsv(tsvPath, sets, filesRoot);
            Console.WriteLine($"TSV written: {tsvPath} ({sets.Count} sets)");
            return;
        }

        Console.WriteLine($"Total BeatmapSets in realm: {sets.Count}");
        Console.WriteLine("Recent 40 by DateAdded:");
        foreach (var s in sets.Take(40))
            Console.WriteLine($"  {((DateTimeOffset)s.DateAdded).ToLocalTime():yyyy-MM-dd HH:mm:ss}  online={TryStr(() => (object)s.OnlineID),10}  {Describe(s)}");

        var selected = since.HasValue ? sets.Where(s => ((DateTimeOffset)s.DateAdded) >= since.Value).ToList() : sets.ToList();
        if (idsFile != null)
        {
            var wanted = new HashSet<string>(File.ReadAllLines(idsFile).Select(l => l.Trim()).Where(l => l.Length > 0), StringComparer.OrdinalIgnoreCase);
            selected = sets.Where(s => wanted.Contains(TryStr(() => (object)s.ID))).ToList();
        }
        Console.WriteLine($"\nSelected: {selected.Count}");

        if (dumpOnly)
        {
            Directory.CreateDirectory(outDir);
            WriteManifest(Path.Combine(outDir, "manifest.json"), selected);
            Console.WriteLine("Manifest written to " + Path.Combine(outDir, "manifest.json"));
            return;
        }

        Directory.CreateDirectory(outDir);
        var report = new List<string>();
        var usedNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        int ok = 0, failed = 0;

        foreach (var s in selected)
        {
            string name = BuildFileName(s, usedNames);
            string target = Path.Combine(outDir, name);
            try
            {
                long bytes = WriteOsz(s, filesRoot, target);
                ok++;
                Console.WriteLine($"OK   {name}  ({bytes / 1024.0 / 1024.0:F1} MB)");
                report.Add($"{name}\t{bytes}\t{((DateTimeOffset)s.DateAdded).ToLocalTime():O}");
            }
            catch (Exception e)
            {
                failed++;
                Console.WriteLine($"FAIL {name}: {e.Message}");
            }
        }

        File.WriteAllLines(Path.Combine(outDir, "export-report.tsv"),
            new[] { "file\tbytes\tdateAdded" }.Concat(report), new UTF8Encoding(false));
        WriteManifest(Path.Combine(outDir, "manifest.json"), selected);
        Console.WriteLine($"\nDone. OK={ok} FAIL={failed} -> {outDir}");
    }

    static string Describe(dynamic s)
    {
        dynamic b = null;
        try { b = s.Beatmaps[0]; } catch { }
        string artist = "", title = "", creator = "";
        if (b != null)
        {
            artist = TryStr(() => (object)b.Metadata.Artist);
            title = TryStr(() => (object)b.Metadata.Title);
            creator = TryStr(() => (object)b.Metadata.Author.Username);
        }
        int files = 0, beatmaps = 0;
        try { files = (int)s.Files.Count; } catch { }
        try { beatmaps = (int)s.Beatmaps.Count; } catch { }
        return $"{artist} - {title} ({creator}) | {files} files, {beatmaps} diffs";
    }

    static string BuildFileName(dynamic s, HashSet<string> used)
    {
        dynamic b = null;
        try { b = s.Beatmaps[0]; } catch { }
        string artist = b == null ? "" : TryStr(() => (object)b.Metadata.Artist);
        string title = b == null ? "" : TryStr(() => (object)b.Metadata.Title);
        string creator = b == null ? "" : TryStr(() => (object)b.Metadata.Author.Username);
        string baseName = $"{artist} - {title} ({creator})";
        baseName = Sanitize(baseName);
        if (string.IsNullOrWhiteSpace(baseName))
            baseName = "beatmapset_" + s.ID;

        string candidate = baseName + ".osz";
        int n = 2;
        while (!used.Add(candidate))
            candidate = $"{baseName} ({n++}).osz";
        return candidate;
    }

    static string Sanitize(string s)
    {
        var sb = new StringBuilder();
        foreach (char c in s)
        {
            if (char.IsControl(c) || Invalid.Contains(c.ToString()))
                sb.Append('_');
            else
                sb.Append(c);
        }
        string r = Regex.Replace(sb.ToString(), @"\s+", " ").Trim().TrimEnd('.');
        if (r.Length > 150) r = r.Substring(0, 150).TrimEnd();
        return r;
    }

    static long WriteOsz(dynamic set, string filesRoot, string target)
    {
        var entries = new List<(string Name, string Hash)>();
        dynamic files = set.Files;
        int count = (int)files.Count;
        for (int i = 0; i < count; i++)
        {
            dynamic f = files[i];
            string filename = (string)f.Filename;
            string hash = (string)f.File.Hash;
            if (string.IsNullOrEmpty(hash)) continue;
            entries.Add((filename.Replace('\\', '/'), hash));
        }

        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        using var fs = new FileStream(target, FileMode.Create, FileAccess.Write, FileShare.None);
        using var zip = new ZipArchive(fs, ZipArchiveMode.Create);
        long total = 0;
        var missing = new List<string>();

        foreach (var e in entries)
        {
            string name = e.Name;
            if (!seen.Add(name))
            {
                string dir = Path.GetDirectoryName(name)?.Replace('\\', '/') ?? "";
                string stem = Path.GetFileNameWithoutExtension(name);
                string ext = Path.GetExtension(name);
                int k = 2;
                string alt;
                do { alt = (dir.Length > 0 ? dir + "/" : "") + stem + "_" + k++ + ext; } while (!seen.Add(alt));
                name = alt;
            }

            string src = Path.Combine(filesRoot, e.Hash.Substring(0, 1), e.Hash.Substring(0, 2), e.Hash);
            if (!File.Exists(src))
            {
                missing.Add(e.Name);
                continue;
            }

            var entry = zip.CreateEntry(name, CompressionLevel.Optimal);
            using (var es = entry.Open())
            using (var ss = File.OpenRead(src))
            {
                ss.CopyTo(es);
                total += ss.Length;
            }
        }

        if (missing.Count > 0)
        {
            zip.Dispose();
            fs.Dispose();
            File.Delete(target);
            throw new InvalidOperationException("missing " + missing.Count + " file(s) in store: " + string.Join(", ", missing.Take(5)));
        }

        return total;
    }

    static string Cell(string value)
    {
        if (string.IsNullOrEmpty(value)) return "";
        return value.Replace('\t', ' ').Replace('\r', ' ').Replace('\n', ' ');
    }

    static void WriteTsv(string path, List<dynamic> sets, string filesRoot)
    {
        var sb = new StringBuilder();
        sb.Append("id\tdateAdded\tartist\ttitle\tcreator\tdiffs\tfiles\tbytes\taudio\n");
        foreach (var s in sets)
        {
            dynamic b = null;
            try { b = s.Beatmaps[0]; } catch { }
            string artist = b == null ? "" : TryStr(() => (object)b.Metadata.Artist);
            string title = b == null ? "" : TryStr(() => (object)b.Metadata.Title);
            string creator = b == null ? "" : TryStr(() => (object)b.Metadata.Author.Username);
            string audio = b == null ? "" : TryStr(() => (object)b.Metadata.AudioFile);
            int diffs = 0, files = 0;
            long bytes = 0;
            var modeCounts = new int[4];
            try
            {
                dynamic bm = s.Beatmaps;
                diffs = (int)bm.Count;
                for (int i = 0; i < diffs; i++)
                {
                    dynamic one = bm[i];
                    int mode = 0;
                    try { mode = (int)one.Ruleset.OnlineID; } catch { }
                    if (mode < 0 || mode > 3) mode = 0;
                    modeCounts[mode]++;
                }
            }
            catch { }
            try
            {
                dynamic fl = s.Files;
                files = (int)fl.Count;
                for (int i = 0; i < files; i++)
                {
                    dynamic f = fl[i];
                    string hash = (string)f.File.Hash;
                    if (string.IsNullOrEmpty(hash)) continue;
                    string p = Path.Combine(filesRoot, hash.Substring(0, 1), hash.Substring(0, 2), hash);
                    var fi = new FileInfo(p);
                    if (fi.Exists) bytes += fi.Length;
                }
            }
            catch { }

            sb.Append(TryStr(() => (object)s.ID)).Append('\t')
              .Append(((DateTimeOffset)s.DateAdded).ToLocalTime().ToString("yyyy-MM-dd HH:mm:ss")).Append('\t')
              .Append(Cell(artist)).Append('\t')
              .Append(Cell(title)).Append('\t')
              .Append(Cell(creator)).Append('\t')
              .Append(diffs).Append('\t')
              .Append(files).Append('\t')
              .Append(bytes).Append('\t')
              .Append(Cell(audio)).Append('\t')
              .Append(modeCounts[0]).Append('\t')
              .Append(modeCounts[1]).Append('\t')
              .Append(modeCounts[2]).Append('\t')
              .Append(modeCounts[3]).Append('\n');
        }
        File.WriteAllText(path, sb.ToString(), new UTF8Encoding(false));
    }

    static void WriteManifest(string path, List<dynamic> sets)
    {
        var list = new List<object>();
        foreach (var s in sets)
        {
            dynamic b = null;
            try { b = s.Beatmaps[0]; } catch { }
            var files = new List<object>();
            try
            {
                dynamic fl = s.Files;
                for (int i = 0; i < (int)fl.Count; i++)
                {
                    dynamic f = fl[i];
                    files.Add(new { name = TryStr(() => (object)f.Filename), hash = TryStr(() => (object)f.File.Hash) });
                }
            }
            catch { }

            list.Add(new
            {
                id = TryStr(() => (object)s.ID),
                onlineId = TryStr(() => (object)s.OnlineID),
                dateAdded = TryStr(() => (object)((DateTimeOffset)s.DateAdded).ToLocalTime().ToString("O")),
                artist = b == null ? "" : TryStr(() => (object)b.Metadata.Artist),
                title = b == null ? "" : TryStr(() => (object)b.Metadata.Title),
                creator = b == null ? "" : TryStr(() => (object)b.Metadata.Author.Username),
                audio = b == null ? "" : TryStr(() => (object)b.Metadata.AudioFile),
                background = b == null ? "" : TryStr(() => (object)b.Metadata.BackgroundFile),
                files
            });
        }
        File.WriteAllText(path, JsonSerializer.Serialize(list, new JsonSerializerOptions { WriteIndented = true }), new UTF8Encoding(false));
    }
}
