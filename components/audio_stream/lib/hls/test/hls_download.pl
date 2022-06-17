#!/usr/bin/perl

use strict;
use warnings;

use LWP::UserAgent ();
use URI        ();
use HTTP::Date ();
use Encode;
use Encode::Locale;
use Getopt::Long;
use File::Path qw(make_path);
use threads;

my $force_download = 0;
my $m3u8;
my $bitrate = 0;
my $top_dir = 'hls';
my %play_list;
my %file_list;
my @files;
$SIG{INT} = sub { die "Interrupted\n"; };
$| = 1;         # autoflush
GetOptions(
    'force!'  => \$force_download,
    'url:s' => \$m3u8,
    'bitrate:i' => \$bitrate,
    'dir:s' => \$top_dir,
);
unless ($m3u8) {
print  << 'USAGE';
-force force overwrite file when exists
-url set m3u8 path
-dir set download path
-bitrate set accept bitrate
./hls_download.pl -url https://bitdash-a.akamaihd.net/content/sintel/hls/playlist.m3u8 -bitrate 1144430 -dir hls
USAGE

    exit(0);
}

my $ua = LWP::UserAgent->new(
    agent      => "lwp-download/1.0 ",
    keep_alive => 1,
    env_proxy  => 1);

my $dir = "$top_dir/" . get_file_name($m3u8);
print "Got dir $dir $m3u8\n";
unless (download_url($m3u8, $dir) == 1 || -e $dir) {
    die "fail to download $m3u8\n";
}
get_file_lists($dir);

my $file_num = @files;
my $seg_num  = int($file_num / 5) + 1;
my @thread;
if ($seg_num > 10) {
    $seg_num = 10;
}

for (0..$seg_num-1) {
    push @thread, threads->create(\&download_segments, get_segment($_));
}

for (@thread) {
    $_->join();
}

sub get_segment {
    my $idx = shift;
    my $n = int ($file_num / $seg_num);
    return [$n* $idx, $idx + 1 < $seg_num ? ($idx+1)*$n-1 : $file_num-1];
}

sub download_segments {
    my $seg = shift;
    for my $idx($seg->[0] .. $seg->[1]) {
        my $remote = $files[$idx];
        my $local  = $file_list{$remote};
        print "$idx: download $remote to $local\n";
        download_url($remote, $local);
    }
}

sub get_file_lists {
    my $dir = shift;
    my $cont = read_file($dir);
    if ($cont =~/(EXT-X-STREAM-INF|EXT-X-MEDIA):/ms) {
        print "This is main playlist\n";
        parse_main_playlist($cont);
        for my $p(keys %play_list) {
            my $remote = get_full_url($m3u8, $p);
            my $local = get_local_path($top_dir, $p);
            $play_list{$remote} = check_dir($local);
            if (download_url($remote, $local) || -e $local) {
                $cont = read_file($local);
                parse_playlist($cont, $remote);
            }
        }
    }
    else {
        $play_list{$m3u8} = $top_dir;
        parse_playlist($cont, $m3u8);
    }
}

sub parse_main_playlist {
    my $s = shift;
    my $stream_inf = 0;
    my $attr = {};
    my @media;
    my @stream;
    while ($s =~/(.*?)[\r\n]+/msg) {
        if ($stream_inf) {
            $attr->{"URI"} = $1;
            print "Got Stream uri ", $attr->{"URI"}, "\n";
            $stream_inf = 0;
            next;
        }
        my $l = $1;
        ##EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="stereo",LANGUAGE="en",NAME="English",DEFAULT=YES,AUTOSELECT=YES,URI="audio/stereo/en/128kbit.m3u8"
        if ($l =~/#EXT-X-MEDIA:(.*)/) {
            $attr = get_attr($1);
            push @media, $attr;
            print "Got media uri ", $attr->{"URI"}, "\n";
        }
        ##EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=520929,CODECS="avc1.4d4015,mp4a.40.2",AUDIO="stereo",RESOLUTION=638x272,SUBTITLES="subs"
        elsif ($l=~ /#EXT-X-STREAM-INF:(.*)/) {
            $attr = get_attr($1);
            push @stream, $attr;
            $stream_inf = 1;
        }
    }
    
    #get all playlist which match bitrate
    @stream = sort {$a->{"BANDWIDTH"} <=> $b->{"BANDWIDTH"}} @stream;
    my $sel = sub {
        my $s = shift;
        my @type = qw /AUDIO SUBTITLES/;
        if (exists $s->{"URI"}) {
            print "match...", $s->{"URI"},"\n";
            $play_list{$s->{"URI"}} = 1;
        }
        for my $t(@type) {
            next unless (exists $s->{$t});
            my $group_id = $s->{$t};
            for my $m(@media) {
                next unless (exists $m->{"URI"});
                if ($m->{"TYPE"} eq $t &&  $m->{"GROUP-ID"} eq $group_id) {
                    $play_list{$m->{"URI"}} = 1;
                    print "match...", $m->{"URI"},"\n";
                }
            }
        }
    };
    for my $m(@stream) {
        if ($m->{"BANDWIDTH"} < $bitrate) {
            next;
        }
        $sel->($m);
        if ($m->{"BANDWIDTH"} >= $bitrate) {
            last;
        }
    }
}

sub get_attr {
    my $s = shift;
    my $attr = {};
    while ($s =~/([\w-]+)=["]?([^,"]+)/g) {
        $attr->{$1} = $2;
    }
    return $attr;
}

sub parse_playlist {
    my $d = shift;
    my $remote = shift;
    my $dir = $play_list{$remote};
    my @info = split(/#EXTINF:/, $d);
    for $d(@info) {
        my @l = split(/[\r\n]+/, $d);
        my $p;
        for (1..$#l) {
           if ($l[$_] =~/\w+/) {
              unless ($l[$_] =~/^#EXT/) {
                  $p = $l[$_];
                  last;
              }
           }
        }
        if ($p) {
	    my $r = get_full_url($remote, $p);
	    my $local = get_local_path($dir, $p);
	    $file_list{$r} = $local;
	    push @files, $r;
	    check_dir($local);
	    print "Remote $r to $local\n";
        }
    }
}

sub check_dir {
    my $p = shift;
    if ($p =~/(.*)\//) {
        my $dir = $1;
        unless (-d $dir) {
            make_path($dir);
        }
        return $dir;
    }
    return "";
}

sub get_full_url {
    my $base = shift;
    my $ext  = shift;
    if ($ext =~/http/) {
        return $ext;
    }
    my @base = split('\/', $base);
    my @ext = split('\/', $ext);
    pop @base;
    while ($#ext >= 0) {
        if ($ext[0] eq '.') {
            shift @ext;
            next;
        }
        if ($ext[0] eq '..') {
            shift @ext;
            pop @base;
            next;
        }
        push @base, @ext;
        last;
    }
    return join('/', @base);
}

sub get_local_path {
    my $dir = shift;
    my $ext = shift;
    if ($ext =~/http/) {
        return "$dir/" . get_file_name($ext);
    }
    return "$dir/$ext";
}

sub get_file_name {
    my $url = shift;
    if ($url =~/.*\/([\w\.-]+)/) {
        return $1;
    }
    return $url;
}

sub read_file {
    my $f = shift;
    open(my $H, $f) || return "";
    my $str;
    do {local $/=undef; $str = <$H>;};
    close $H;
    return $str;
}

sub download_url {
    my $url = shift;
    my $dir = shift;
    my $name = get_file_name($url);
    if (-e $dir) {
        if ($force_download == 0) {
            print "$dir already exists if you still want to overwrite on it using -f option\n";
            return 0;
        }
    }
  
    my $H;
    my $start_time = 0;
    my $total = 0;
    my $last_time;
    my $res = $ua->request(
        HTTP::Request->new(GET => $url),
        sub {
            my $r = $_[1];
            my $cur = time;
            unless ($H) {
                print "Start to download $url to $dir\n";
                open($H, "+>", $dir) || die "Can't open $dir: $!\n";
                binmode $H;
                $start_time = $cur;
                $last_time = $cur;
            }
            print $H $_[0];
            $total += length($_[0]);
            if ($cur >= $last_time + 5) {
                $last_time = $cur;
                print "$name $total / $r->content_length\n";
            }
        }
    );
    if ($H) {
        close($H);
    }
    if (!$res->is_success) {
        print "Fail to download for $res->status_line\n";
        return 0;
    }
    return 1;
}
