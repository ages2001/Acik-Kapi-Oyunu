#version 430
layout(binding=0) uniform sampler2D imageSampler;
layout(location=0) out vec4 outColor;
void main()
{
    outColor = textureLod(imageSampler, gl_FragCoord.xy * (1.0 / 32.0), 0.0);
}
