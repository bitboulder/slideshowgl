#!/usr/bin/perl

use strict;

shift if my $nodo=$ARGV[0]eq"-nodo";

my $file=shift;
my $cmd=shift;
my $img=shift;
my $frmi=shift;
my @arg=@ARGV;

open FD,"<".$file;
my @file=<FD>;
close FD;

if($cmd=~/^add/){
	my $frm=-1;
	my $pos=-1;
	for(my $i=0;$i<@file;$i++){
		next if $file[$i]!~/^frm/;
		next if (++$frm)!=$frmi+1;
		$pos=$i;
	}
	if($pos<0){
		while($frm!=$frmi){
			push @file,"frm\n";
			$frm++;
		}
		$pos=@file;
	}
	my @fn=splice @file,0,$pos;
	if("add"eq$cmd){
		push @fn,"img $img\n";
		push @fn,"pos :0.5\n";
	}elsif("addtxt"eq$cmd){
		push @fn,"txt \"$img\"\n";
		push @fn,"col 0xff9900\n";
		push @fn,"pos :1\n";
	}
	push @fn,@file;
	@file=@fn;
}elsif($cmd=~/^frm/){
	my $frm=-1;
	my $str=-1;
	my $end=-1;
	for(my $i=0;$i<@file;$i++){
		$frm++ if $file[$i]=~/^frm/;
		next if $frm!=$frmi;
		$str=$i if $str<=0;
		$end=$i;
	}
	splice @file,$str,$end-$str+1 if "frmdel"eq$cmd;
	if("frmins"eq$cmd){
		my @fn=splice @file,0,$str;
		push @fn,"frm\n";
		push @fn,@file;
		@file=@fn;
	}
}elsif(""ne$img){
	my $frm=-1;
	my $str=-1;
	my $end=-1;
	for(my $i=0;$i<@file;$i++){
		$frm++ if $file[$i]=~/^frm/;
		next if $frm!=$frmi;
		last if $str>=0 && $file[$i]=~/^(img|txt)/;
		$str=$i if $file[$i]=~/^img[[:blank:]]+$img/;
		$str=$i if $file[$i]=~/^txt[[:blank:]]+\"$img\"/;
		next if $str<0;
		$end=$i;
		&poschg($i) if $cmd=~/^scale/;
		&posset($i) if $cmd eq "pos";
	}
	splice @file,$str,$end-$str+1 if $str>=0 && "del"eq$cmd;
}

if($nodo){
	print "".join "",@file;
}else{
	open FD,">".$file;
	foreach(@file){ print FD $_; }
	close FD;
}

sub poschg {
	my @defval=(1,1,0,0,0);
	my @valmax=(1,8, 0.5, 0.5,0);
	my @valmin=(0,0,-0.5,-0.5,0);
	my $i=shift;
	return if $file[$i]!~/^((pos|src|dst|sd)[[:blank:]]+)([0-9.:+-]+)(.*)$/;
	my $pre=$1;
	my $suf=$4;
	my @val=split /:/,$3;
	my $vi=-1;
	$vi=1 if $cmd=~/^scale/;
	return if $vi<0;
	$val[$vi]=$defval[$vi] if ""eq$val[$vi];
	$val[1]*=sqrt(2) if "scaleinc"eq$cmd;
	$val[1]/=sqrt(2) if "scaledec"eq$cmd;
	$val[$vi]=sprintf "%.3f",$val[$vi];
	$val[$vi]=$valmax[$vi] if $val[$vi]>$valmax[$vi];
	$val[$vi]=$valmin[$vi] if $val[$vi]<$valmin[$vi];
	$file[$i]=$pre.(join ":",@val).$suf."\n";
}

sub posset {
	my @valmax=(1,8, 0.5, 0.5,0);
	my @valmin=(0,0,-0.5,-0.5,0);
	my $i=shift;
	return if $file[$i]!~/^((pos|src|dst|sd)[[:blank:]]+)([0-9.:+-]+)(.*)$/;
	my $pre=$1;
	my $suf=$4;
	my @val=split /:/,$3;
	for(my $i=0;$i<2;$i++){
		$val[2+$i]=$arg[$i];
		$val[2+$i]=$valmax[2+$i] if $val[2+$i]>$valmax[2+$i];
		$val[2+$i]=$valmin[2+$i] if $val[2+$i]<$valmin[2+$i];
	}
	$file[$i]=$pre.(join ":",@val).$suf."\n";
}
