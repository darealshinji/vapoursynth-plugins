#!/usr/bin/env perl

use strict;
use warnings;
use bigint;
use File::Spec;
use JSON::XS;

my $filename = $ARGV[0];
my $data = `ffprobe -show_streams -show_format -show_frames -of json $filename`;
my $input = decode_json($data);

my @frames = @{$input->{frames}};
my @streams = @{$input->{streams}};
my $format = $input->{format};

my @videoframes;
my $video;
my $audio;

foreach my $frame (@frames) {
    if ($frame->{media_type} eq "video") {
        push(@videoframes, $frame);
    }
}

foreach my $stream (@streams) {
    if ($stream->{codec_type} eq "audio") {
        $audio = $stream;
    } elsif ($stream->{codec_type} eq "video") {
        $video = $stream;
    }
}

print("DGIndexProjectFile16\n");
print("1\n");
print(File::Spec->rel2abs($filename)."\n\n");

if ($format->{format_name} eq "mpeg") {
    print("Stream_Type=1\n");
} elsif ($format->{format_name} eq "mpegts") {
    print("Stream_type=2\n");

    my $pids = "MPEG2_Transport_PID=";
    foreach my $stream (@streams) {
        my $pid = $stream->{id};
        $pid =~ s/^0x//;
        $pids .= "$pid,";
    }
    $pids =~ s/.$//;
    print("$pids,000\n");

    print("Transport_Packet_Size=188\n");
} else {
    die("Invalid Stream");
}

if ($video->{codec_name} eq "mpeg1video") {
    print("MPEG_Type=1\n");
} elsif ($video->{codec_name} eq "mpeg2video") {
    print("MPEG_Type=2\n");
} else {
    die("Invalid Stream");
}

print("iDCT_Algorithm=6\n");
print("YUVRGB_Scale=1\n");
print("Luminance_Filter=0,0\n");
print("Clipping=0,0,0,0\n");
print("Aspect_Ratio=$video->{display_aspect_ratio}\n");
print("Picture_Size=$video->{width}x$video->{height}\n");
print("Field_Operation=0\n");

my @fps = split(/\//, $video->{r_frame_rate});
my $ffps = $fps[0] * 1000 / $fps[1];
print("Frame_Rate=$ffps ($video->{r_frame_rate})\n");
print("Location=0,0,0,0\n");

my @yarr = sort { $a->{coded_picture_number} <=> $b->{coded_picture_number} } @videoframes;
my @need;
my $lastpts = 0;
foreach my $y (@yarr) {
    if ($y->{pict_type} eq "I") {
        $lastpts = $y->{pkt_dts};
    }
    if (defined($y->{pkt_dts}) && $y->{pkt_dts} < $lastpts) {
        $need[$y->{coded_picture_number}] = 1;
    } else {
        $need[$y->{coded_picture_number}] = 0;
    }
}

my @gopstart;
my $i = 0;
foreach my $frame (@videoframes) {
    $gopstart[$i]->{isgop} = 0;
    if ($frame->{pict_type} eq "I" && $i != 0) {
        my $j = $i;
        while($frame->{coded_picture_number} <= $videoframes[$j]->{coded_picture_number} && $j > 0) {
            $j--;
        }
        $j++;
        $gopstart[$j]->{isgop} = 1;
        if (defined($videoframes[$i]->{pkt_pos})) {
            $gopstart[$j]->{pos} = $videoframes[$i]->{pkt_pos};
        } else {
            my $k = $i;
            while (!defined($videoframes[$k]->{pkt_pos})) {
                $k--;
            }
            $gopstart[$i]->{pos} = $videoframes[$k]->{pkt_pos};
        }
    }
    $i++;
}

$gopstart[0]->{isgop} = 1;
$gopstart[0]->{pos} = $videoframes[0]->{pkt_pos};

$i = 0;
foreach my $frame (@videoframes) {
    my $flag = 0;

    if ($gopstart[$i]->{isgop} == 1) {
        print("\n900 1 0 ");
        print($gopstart[$i]->{pos});
        print(" 0 0 0");
    }

    if ($frame->{pict_type} eq "I") {
        $flag += 0x10;
    } elsif ($frame->{pict_type} eq "P") {
        $flag |= 0x20;
    } elsif ($frame->{pict_type} eq "B") {
        $flag |= 0x30;
    } else {
        die("Invalid Stream");
    }

    if (!($need[$frame->{coded_picture_number}] == 1)) {
        $flag |= 0x80;
    }

    if ($frame->{top_field_first} == 1) {
        $flag |= 0x2;
    }

    if ($frame->{repeat_pict} != 0) {
        $flag |= 0x1;
    }

    if ($frame->{interlaced_frame} == 0) {
        $flag |= 0x40;
    }

    $i++;

    print(lc(sprintf(" %02X", $flag)));
}

print(" ff\n");