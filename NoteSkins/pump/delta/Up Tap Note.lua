local t = Def.ActorFrame{}


t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(diffuseshift;effectcolor1,color("#376f9b66");effectcolor2,color("#376f9b66");rotationz,45);
}

t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(blend,Blend.Add;diffuseshift;effectcolor1,color("#6600ccFF");effectcolor2,color("#6600cc66");effectclock,"bgm";effecttiming,1,0,0,0;rotationz,45);
}

t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(diffuseshift;effectcolor1,color("#6600ccFF");effectcolor2,color("#6600ccFF");fadetop,1;rotationz,45);
}

t[#t+1] = LoadActor("UpLeft_fill")..{
	InitCommand=cmd(blend,Blend.Add;diffuseshift;effectcolor1,color("#6600ccFF");effectcolor2,color("#6600ccFF");rotationz,45);
}

return t