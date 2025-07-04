vec3 blinnPhong(vec3 diffuse, vec3 n, vec3 h)
{
    vec3 specular = diffuse * 0.5;
    float NoH = clamp(dot(n, h), 0, 1);
    specular *= pow(NoH, 32);

    return diffuse + specular;
}