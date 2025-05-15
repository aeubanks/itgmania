local t = Def.ActorFrame{}


t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(diffuseshift;effectcolor1,color("#376f9b66");effectcolor2,color("#376f9b66");rotationz,-45);
}

t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(blend,Blend.Add;diffuseshift;effectcolor1,color("#009944FF");effectcolor2,color("#00994466");effectclock,"bgm";effecttiming,1,0,0,0;rotationz,-45);
}

t[#t+1] = LoadActor("UpLeft_blend")..{
	InitCommand=cmd(diffuseshift;effectcolor1,color("#009944FF");effectcolor2,color("#009944FF");fadetop,1;rotationz,-45);
}

t[#t+1] = LoadActor("UpLeft_fill")..{
	InitCommand=cmd(blend,Blend.Add;diffuseshift;effectcolor1,color("#009944FF");effectcolor2,color("#009944FF");rotationz,-45);
}

return t