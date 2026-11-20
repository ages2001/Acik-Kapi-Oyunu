#version 430
layout(location=0) out vec4 outColor;
void main()
{
    vec2 xy = gl_FragCoord.xy * (1.0 / 32.0);
    outColor = vec4(xy.x, xy.y, (xy.x + xy.y) * 0.5, 1.0);
}
