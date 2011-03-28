#!/usr/bin/perl

use strict;

shift if my $nodo=$ARGV[0]eq"-nodo";

my $file=shift;
my $cmd=shift;
my $imgfn=shift;
my $frmi=shift;
my @arg=@ARGV;

my @prg=&loadprg($file);
my $chg=0;
   if("frmins"eq$cmd){ $chg=&frmins(\@prg,$frmi,&cfrm()); }
elsif("frmdel"eq$cmd){ $chg=&frmdel(\@prg,$frmi); }
elsif("frmmov"eq$cmd){ $chg=&frmmov(\@prg,$frmi,$arg[0]); }
elsif("frmcpy"eq$cmd){ $chg=&frmcpy(\@prg,$frmi,$arg[0]); }
elsif("imgadd"eq$cmd){ $chg=&imgadd(\@prg,$frmi,&cimg($imgfn)); }
elsif("txtadd"eq$cmd){ $chg=&imgadd(\@prg,$frmi,&ctxt($imgfn)); }
elsif("imgdel"eq$cmd){ $chg=&imgdel(\@prg,$frmi,$imgfn); }
elsif("imgpos"eq$cmd){ $chg=&imgpos(\@prg,$frmi,$imgfn,"pos",$arg[0]); }
elsif("imgon" eq$cmd){ $chg=&imgpos(\@prg,$frmi,$imgfn,"on", $arg[0]); }
elsif("imgoff"eq$cmd){ $chg=&imgpos(\@prg,$frmi,$imgfn,"off",$arg[0]); }
elsif("imgcol"eq$cmd){ $chg=&imgcol(\@prg,$frmi,$imgfn,$arg[0]); }
my $prg=&joinprg(@prg);
&saveprg($file,$prg) if $chg && !$nodo;
print $prg if $nodo;
exit 0;

sub frmins {
	my $prg=shift;
	my $frmi=shift;
	my $frm=shift;
	return 0 if $frmi<0 || $frmi>=@{$prg};
	for(my $i=@{$prg};$i>$frmi+1;$i--){
		$prg->[$i]=$prg->[$i-1];
	}
	$prg[$frmi+1]=$frm;
	return 1;
}

sub frmdel {
	my $prg=shift;
	my $frmi=shift;
	return 0 if $frmi<0 || $frmi>=@{$prg}-1;
	return splice @{$prg},$frmi+1,1;
}

sub frmmov {
	my $prg=shift;
	my $fsrc=shift;
	my $fdst=shift;
	return 0 if ""eq$fdst;
	my $frm=&frmdel($prg,$fsrc);
	return 0 if !$frm;
	return &frmins($prg,$fdst,$frm);
}

sub frmcpy {
	my $prg=shift;
	my $fsrc=shift;
	my $fdst=shift;
	return 0 if ""eq$fdst;
	my $frm=&frmfind($prg,$fsrc);
	return 0 if !$frm;
	return &frmins($prg,$fdst,$frm);
}

sub imgadd {
	my $prg=shift;
	my $frmi=shift;
	my $img=shift;
	my $frm=&frmfind($prg,$frmi);
	return 0 if !$frm;
	push @{$frm->{"img"}},$img;
	return 1;
}

sub imgdel {
	my $prg=shift;
	my $frmi=shift;
	my $img=shift;
	my @img=&imgfind($prg,$frmi,$imgfn);
	return 0 if !@img;
	foreach my $img (@img){
		@{$img}=();
	}
	return 1;
}

sub imgpos {
	my $prg=shift;
	my $frmi=shift;
	my $imgfn=shift;
	my $key=shift;
	my $pos=shift;
	my @img=&imgfind($prg,$frmi,$imgfn);
	return 0 if !@img;
	foreach my $img (@img){
		&imgposi($img,$key,$pos);
	}
	return 1;
}

sub imgposi {
	my $img=shift;
	my $key=shift;
	my $pos=shift;
	my @posmin=$key=~/on|off/ ? (0,0,0) : (0,0,-0.5,-0.5,0);
	my @posmax=$key=~/on|off/ ? (1,1,2) : (1,8, 0.5, 0.5,0);
	my @pos=split /:/,$pos;
	for(my $i=0;$i<@pos;$i++){
		next if ""eq$pos[$i];
		$pos[$i]=$posmax[$i] if $pos[$i]>$posmax[$i];
		$pos[$i]=$posmin[$i] if $pos[$i]<$posmin[$i];
	}
	my $done=0;
	foreach my $line (@{$img}){
		next if $line!~/^([ \t]*($key)[ \t]+)([0-9.:+-]+)([ \t\n\r]*)$/;
		my $pre=$1;
		my $suf=$4;
		my @val=split /:/,$3;
		for(my $i=0;$i<@pos;$i++){
			$val[$i]=$pos[$i] if $pos[$i]ne"";
		}
		$line=$pre.(join ":",@val).$suf;
		$done=1;
	}
	push @{$img},$key." ".(join ":",@pos)."\n" if !$done;
}

sub imgcoli {
	my $img=shift;
	my $col=shift;
	my $done=0;
	foreach my $line (@{$img}){
		next if $line!~/^([ \t]*col[ \t]+)([0-9a-fx]+)([ \t\n\r]*)$/;
		my $pre=$1;
		my $suf=$3;
		$line=$pre.$col.$suf;
		$done=1;
	}
	push @{$img},"col ".$col."\n" if !$done;
}

sub imgcol {
	my $prg=shift;
	my $frmi=shift;
	my $imgfn=shift;
	my $col=shift;
	my @img=&imgfind($prg,$frmi,$imgfn);
	return 0 if !@img;
	foreach my $img (@img){
		&imgcoli($img,$col);
	}
	return 1;
}

sub frmfind {
	my $prg=shift;
	my $frmi=shift;
	return 0 if $frmi<0 || $frmi>=@{$prg}-1;
	return $prg->[$frmi+1];
}

sub imgfind {
	my $prg=shift;
	my $frmi=shift;
	my $imgfn=shift;
	my @img=();
	my $frm=&frmfind($prg,$frmi);
	return @img if !exists $frm->{"img"};
	my @imgs=@{$frm->{"img"}};
	my @imgfn=split /\//,$imgfn;
	while(!@img && @imgfn){
		$imgfn=join "/",@imgfn;
		foreach my $img (@imgs){
			push @img,$img if $img->[0]=~/^[ \t]*img[ \t]+(.*)[ \t\n\r]*$/ && $1 eq $imgfn;
			push @img,$img if $img->[0]=~/^[ \t]*txt[ \t]+\"(.*)\"[ \t\n\r]*$/ && $1 eq $imgfn;
		}
		shift @imgfn;
	}
	return @img;
}

sub cfrm {
	my %frm=();
	@{$frm{"txt"}}=("frm\n");
	return \%frm;
}

sub cimg {
	my $imgfn=shift;
	my @img=();
	push @img,"img ".$imgfn."\n";
	push @img,sprintf "pos :0.5:%.3f:%.3f\n",(rand()-0.5)/4,(rand()-0.5)/4;
	return \@img;
}

sub ctxt {
	my $imgfn=shift;
	my @img=();
	push @img,"txt \"".$imgfn."\"\n";
	push @img,"col 0xff9900\n";
	push @img,sprintf "pos :0.5:%.3f:%.3f\n",(rand()-0.5)/4,(rand()-0.5)/4;
	push @img,"on ::0\n";
	return \@img;
}

sub loadprg {
	my $file=shift;
	my @prg=();
	@{$prg[0]->{"txt"}}=();
	open FD,"<".$file;
	while(<FD>){
		my $frm = @prg-1;
		$frm++ if $_=~/^[ \t]*frm\b/;
		my $img = -1;
		$img = @{$prg[$frm]->{"img"}}-1 if exists $prg[$frm]->{"img"};
		$img++ if $_=~/^[ \t]*(img|txt)\b/;
		if($img<0){
			push @{$prg[$frm]->{"txt"}},$_;
		}else{
			push @{$prg[$frm]->{"img"}->[$img]},$_;
		}
	}
	close FD;
	return @prg;
}

sub saveprg {
	my $file=shift;
	my $prg=shift;
	open FD,">".$file;
	print FD $prg;
	close FD;
}

sub joinprg {
	my @prg=@_;
	my $prg="";
	foreach my $frm (@prg){
		$prg.=join "",@{$frm->{"txt"}};
		next if !exists $frm->{"img"};
		foreach my $img (@{$frm->{"img"}}){
			$prg.=join "",@{$img};
		}
	}
	return $prg;
}

