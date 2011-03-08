#!/usr/bin/perl

## [INPUT-FORMAT] ##
#
# dir  DIR
# start
# img  FILE
# txt  "TEXT"
# col  r:g:b:a           (1:1:1:1)
# col  0xCOL
#
# deffrm on:stay         (1:1)
# defpos a:s:x:y:r       (1:1:0:0:0)
# defsrc a:s:x:y:r       (0::::)
# defdst a:s:x:y:r       (0::::)
# defsd  => defsrc+defdst
# defon  start:end:layer (0:1:0)
# defoff start:end:layer (0:1:0)
#
# frm    on:stay         (deffrm)
# pos    a:s:x:y:r       (defpos)
# src    a:s:x:y:r       (pos defsrc)
# dst    a:s:x:y:r       (pos defdst)
# sd     => src+dst
# on     start:end:layer (defon)
# off    start:end:layer (defoff)
# col    0xNNNNNN

## [OUTPUT-FORMAT]##
#
# frm;NUM-FRM{:ON:STAY}
# FILE;NUM-EV{:START:END:LAYER:SRC-A:S:X:Y:R:DST-A:S:X:Y:R}
# txt_TEXT_R_G_B_A;NUM-EV...

use strict;

my $fprg=shift;

my %def=(
	"frm"=>"1:1",
	"col"=>"1:1:1",
	"pos"=>"1:1:0:0:0",
	"src"=>"0",
	"dst"=>"0",
	"on" =>"0:1:0",
	"off"=>"0:1:0",
);

foreach my $def (keys %def){
	my @arg=split /:/,$def{$def};
	$def{$def}=\@arg;
}

my @prg=&loadprg($fprg);
@prg=&fillprg(@prg);
my %imgs=&compileprg(@prg);
my $flst=&flstimgs(\%imgs);

&outprg(@prg);
&outimgs(\%imgs);
print $flst;



sub outprg {
	my @prg=@_;
	foreach my $frm (@prg){
		printf "frm %s\n",$frm->{"stay"};
		foreach my $img (@{$frm->{"imgs"}}){
			printf " img %s\n",$img->{"file"};
			printf "  pos  %s\n",join ":",@{$img->{"pos"}};
			printf "  src  %s\n",join ":",@{$img->{"src"}};
			printf "  dst  %s\n",join ":",@{$img->{"dst"}};
			printf "  on   %s\n",join ":",@{$img->{"on"}};
			printf "  off  %s\n",join ":",@{$img->{"off"}};
		}
	}
}

sub outimgs {
	my $arg=shift;
	my %imgs=%{$arg};
	foreach my $f (sort keys %imgs){
		printf "img %s\n",$f;
		foreach my $ev (@{$imgs{$f}}){
			printf "    frm %i start %s end %s src %s dst %s\n",@{$ev};
		}
	}
}

sub flstimgs {
	my $arg=shift;
	my %imgs=%{$arg};
	my $flst="";
	foreach my $f (sort keys %imgs){
		$flst.=sprintf "%s;%i",$f,@{$imgs{$f}}+0;
		foreach my $ev (@{$imgs{$f}}){
			my $str=join ":",@{$ev};
			$flst.=":".$str;
			$str=~s/[^:]//g;
			die "wrong number of arguments (".length($str).")" if "frm"ne$f && 13 != length($str);
		}
		$flst.="\n";
	}
	return $flst;
}

sub loadprg {
	my $fprg=shift;
	my $dir="";
	my $img="";
	my @prg=();
	open FD,"<".$fprg || die "open file $fprg failed";
	while(<FD>){
		chomp $_;
		$_=~s/^[[:blank:]]+|[[:blank:]]+$|#.*$//g;
		next if ""eq$_;
		(my $cmd,my $arg)=split /[[:blank:]]+/,$_,2;
		my @arg=split /:/,$arg;
		foreach(@arg){ $_=~s/^[[:blank:]]+|[[:blank:]]+//g; }
		if("dir"eq$cmd){
			$dir=$arg;
		}elsif("start"eq$cmd){
			@prg=();
		}elsif("frm"eq$cmd){
			$prg[@prg]->{"frm"}=\@arg;
			@{$prg[-1]->{"imgs"}}=();
			@{$prg[-1]->{"evs"}}=();
		}elsif($cmd=~/^(img|txt)$/){
			if("txt"eq$cmd){
				$arg=~s/^"|"$//g;
				$arg="txt_".$arg;
			}else{
				$arg=$dir."/".$arg if ""ne$dir && $arg!~/^(\/|[A-Za-z]:\\)/;
			}
			$prg[-1]->{"imgs"}->[@{$prg[-1]->{"imgs"}}]->{"file"}=$arg;
			@{$prg[-1]->{"imgs"}->[-1]->{"pos"}}=();
			@{$prg[-1]->{"imgs"}->[-1]->{"src"}}=();
			@{$prg[-1]->{"imgs"}->[-1]->{"dst"}}=();
			@{$prg[-1]->{"imgs"}->[-1]->{"on" }}=();
			@{$prg[-1]->{"imgs"}->[-1]->{"off"}}=();
		}elsif($cmd=~/^(pos|src|dst|on|off|col)$/){
			@arg=&colread(@arg) if "col"eq$cmd;
			$prg[-1]->{"imgs"}->[-1]->{$cmd}=\@arg;
		}elsif("sd"eq$cmd){
			$prg[-1]->{"imgs"}->[-1]->{"src"}=\@arg;
			$prg[-1]->{"imgs"}->[-1]->{"dst"}=\@arg;
		}elsif($cmd=~/^def(.*)$/){
			my $cmd=$1;
			$cmd="on:off" if "oo"eq$cmd;
			$cmd="src:dst" if "sd"eq$cmd;
			foreach my $cmd (split /:/,$cmd){
				&fillarg(\@arg,$def{$cmd}) if $cmd!~/^(src|dst)$/;
				$def{$cmd}=\@arg;
			}
		}
	}
	close FD;
	return @prg;
}

sub colread {
	my @arg=@_;
	return @arg if "0x"ne substr $arg[0],0,2;
	my $str=$arg[0];
	@arg=();
	for(my $i=0;$i<6;$i++){
		my $c=substr $str,2+$i,1;
		$c=ord($c)-ord("a")+10 if $c=~/^[a-f]$/;
		$c=ord($c)-ord("A")+10 if $c=~/^[A-F]$/;
		die "colread error ($str -> $i: $c)" if $c!~/^[0-9]+$/ || $c>=16;
		$arg[int($i/2)]+=$c/($i%2 ? 255 : 255/16);
	}
	foreach(@arg){ $_=int($_*1000)/1000; }
	return @arg;
}

sub fillprg {
	my @prg=@_;
	for(my $fi=0;$fi<@prg;$fi++){
		my $frm=$prg[$fi];
		&fillarg($frm->{"frm"},$def{"frm"});
		foreach my $img (@{$frm->{"imgs"}}){
			@{$img->{"col"}}=() if !exists $img->{"col"};
			&fillarg($img->{"col"},$def{"col"});
			my $prv=&findimg($fi-1,$img->{"file"},@prg);
			my $nxt=&findimg($fi+1,$img->{"file"},@prg);
			if($prv){
				&fillarg($img->{"pos"},$prv->{"dst"});
				&fillarg($img->{"src"},$prv->{"dst"});
			}else{
				&fillarg($img->{"pos"},$def{"pos"});
				if(@{$img->{"src"}}){
					&fillarg($img->{"src"},$img->{"pos"});
				}else{
					&fillarg($img->{"src"},$def{"src"},$img->{"pos"});
				}
			}
			if($nxt){
				&fillarg($img->{"dst"},$img->{"pos"});
			}else{
				if(@{$img->{"dst"}}){
					&fillarg($img->{"dst"},$img->{"pos"});
				}else{
					&fillarg($img->{"dst"},$def{"dst"},$img->{"pos"});
				}
			}
			&fillarg($img->{"on" },$def{"on"});
			&fillarg($img->{"off"},$def{"off"});
		}
	}
	return @prg;
}

sub findimg {
	my $fi=shift;
	my $file=shift;
	my @prg=@_;
	return 0 if $fi<0 || $fi>=@prg;
	my $frm=$prg[$fi];
	foreach my $img (@{$frm->{"imgs"}}){
		return $img if $img->{"file"}eq$file;
	}
	return 0;
}

sub fillarg {
	my $dst=shift;
	while(@_){
		my $src=shift;
		for(my $i=0;$i<@{$src};$i++){
			$dst->[$i]=$src->[$i] if ""eq$dst->[$i];
		}
	}
}

sub compileprg {
	my @prg=@_;
	my %imgs=();
	my @frm=();
	for(my $fi=0;$fi<=@prg;$fi++){
		if($fi<@prg){
			push @frm,$prg[$fi]->{"frm"};
			foreach my $img (@{$prg[$fi]->{"imgs"}}){
				$img->{"file"}.="_".join "_",@{$img->{"col"}} if "txt_"eq substr $img->{"file"},0,4;
				my $prv=&findimg($fi-1,$img->{"file"},@prg);
				my @path=();
				if($prv){
					&pushpath(\@path,$prv->{"off"},0,@{$prv->{"pos"}});
					&pushpath(\@path,$prv->{"off"},1,@{$prv->{"dst"}});
				}
				&pushpath(\@path,$img->{"on"},0,@{$img->{"src"}});
				&pushpath(\@path,$img->{"on"},1,@{$img->{"pos"}});
				@path=&alignpath(@path);
				&compilepath(\%imgs,$img,$fi,@path);
			}
		}
		if($fi>0){
			foreach my $prv (@{$prg[$fi-1]->{"imgs"}}){
				my $img=&findimg($fi,$prv->{"file"},@prg);
				next if $img;
				my @path=();
				&pushpath(\@path,$prv->{"off"},0,@{$prv->{"pos"}});
				&pushpath(\@path,$prv->{"off"},1,@{$prv->{"dst"}});
				@path=&alignpath(@path);
				&compilepath(\%imgs,$prv,$fi,@path);
			}
		}
	}
	$imgs{"frm"}=\@frm;
	return %imgs;
}

sub pushpath {
	my $path=shift;
	my $time=shift;
	my $timeid=shift;
	my @pos=@_;
	$path->[@{$path}]->{"t"}=$time->[$timeid];
	$path->[      -1]->{"l"}=$time->[2];
	$path->[      -1]->{"p"}=join ":",@pos;
}

sub alignpath {
	my @p=@_;
	my $start=$p[ 0]->{"t"};
	my $end  =$p[-1]->{"t"};
	for(my $pi=1;$pi<@p-1;$pi++){
		my $time=$p[$pi]->{"t"};
		$time="" if $time<=$start;
		$time="" if $time>=$end;
		$p[$pi]->{"t"}=$time;
	}
	for(my $pi=1;$pi<@p;$pi++){
		next if $p[$pi-1]->{"p"}ne$p[$pi]->{"p"};
		(my $pd,my $ph)=($pi,$pi-1);
		($pd,$ph)=($pi-1,$pi) if $pi==@p-1;
		$p[$ph]->{"t"}=$p[$pd]->{"t"} if ""eq$p[$ph]->{"t"};
		splice @p,$pd,1;
		$pi--;
	}
	my $p1=0;
	for(my $p2=1;$p2<@p;$p2++){
		next if ""eq$p[$p2]->{"t"};
		for(my $pi=$p1+1;$pi<$p2;$pi++){
			$p[$pi]->{"t"}=($pi-$p1)/($p2-$p1);
		}
		$p1=$p2;
	}
	return @p;
}

sub compilepath {
	my $imgs=shift;
	my $img=shift;
	my $fi=shift;
	my @path=@_;
	my $f=$img->{"file"};
	@{$imgs->{$f}}=() if !exists $imgs->{$f};
	if(@path==1){
		my @ev=&compileev($fi,$path[0],$path[0]);
		$imgs->{$f}->[@{$imgs->{$f}}]=\@ev;
	}else{
		my $done=0;
		for(my $pi=1;$pi<@path;$pi++){
			next if $path[$pi-1]->{"p"}eq$path[$pi]->{"p"} && ($pi<@path-1 || $done);
			my @ev=&compileev($fi,$path[$pi-1],$path[$pi]);
			$imgs->{$f}->[@{$imgs->{$f}}]=\@ev;
			$done=1;
		}
	}
}

sub compileev {
	my $fi=shift;
	my $psrc=shift;
	my $pdst=shift;
	return ($fi,$psrc->{"t"},$pdst->{"t"},$pdst->{"l"},$psrc->{"p"},$pdst->{"p"});
}
