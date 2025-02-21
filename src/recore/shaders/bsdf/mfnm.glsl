#ifndef MFNM_GLSL
#define MFNM_GLSL

// Fix normal mapping with a special microfacet model
// https://cg.ivd.kit.edu/publications/2017/normalmaps/normalmap.pdf

float pdot(vec3 a, vec3 b) {
  return max(0.0, dot(a, b));
}

float lambda_p(vec3 wp, vec3 wt, vec3 wi) {
  float i_dot_p = pdot(wp, wi);
  float sinTheta = sqrt(max(0, 1 - wp.z * wp.z));
  return i_dot_p / (i_dot_p + pdot(wt, wi) * sinTheta);
}

float MFNM_G1(vec3 wp, vec3 wt, vec3 w) {
  float sinTheta = sqrt(max(0, 1 - wp.z * wp.z));
  return min(1.f, max(0.f, w.z) * max(0.f, wp.z) / (pdot(w, wp) + pdot(w, wt) * sinTheta));
}

// TODO: currently not usable because of changed interface
#if 0

vec3 MFNM_sample(inout RandomSampler rng, ShadingData sd, vec3 V, out vec3 weight) {
  // Tangent frame for geometric normal
  mat3 frameFaceN = tangentFrame(sd.faceN);// geometric shading to world
  mat3 invFrameFaceN = inverse(frameFaceN);// world to geometric shading

  mat3 frameN = tangentFrame(sd.N);// smooth shading to world
  mat3 invFrameN = inverse(frameN);// world to smooth shading

  vec3 wp = invFrameFaceN * sd.N;// perturbed normal in shading space

  if (wp.z <= 0) {
    vec3 V_t = invFrameFaceN * V;
    return sampleBRDF(rngState, sd, V_t, weight);
  }

  vec3 V = invFrameFaceN * V;// incoming direction in shading space
  vec3 wt = normalize(vec3(-wp.x, -wp.y, 0));// perpendicular to faceN
  vec3 wt_ = frameFaceN * wt;// in world space

  vec3 wr = -V;// - incoming direction in world space

  float xi0 = RandomSampler_nextFloat(rng);
  float lambda = lambda_p(wp, wt, -V);
  if (xi0 > lambda) {
    // sample on wp

    vec3 V_t = -invFrameN * wr;// perturbed_wp.wi = -perturbed_wp.toLocal(wr);
    vec3 randomDirection = sampleBRDF(rngState, sd, V_t, weight);// m_nested->sample(query_wp, sample);

    // did sampling fail?
    if (all(lessThanEqual(weight, vec3(0.0)))) return vec3(0.0);

    wr = frameN * randomDirection;// wr = perturbed_wp.toWorld(query_wp.wo);

    float G1 = MFNM_G1(wp, wt, invFrameFaceN * wr);// float G1_ = G1(wp, bRec.its.toLocal(wr));

    // is the sampled direction shadowed?
    float xi1 = RandomSampler_nextFloat(rng);
    if (xi1 > G1) {
      // reflect on wt
      wr = reflect(wr, wt_);

      G1 = MFNM_G1(wp, wt, invFrameFaceN * wr);
      // just write evaluation as inverse into the pdf output term
      weight *= G1;// energy *= G1(wp, bRec.its.toLocal(wr));
    }

  } else {
    //    return vec3(0, 0, -100);
    // do one reflection if we start at wt
    wr = reflect(wr, wt_);

    // sample on wp

    vec3 V_t = -invFrameN * wr;// perturbed_wp.wi = -perturbed_wp.toLocal(wr);
    vec3 randomDirection = sampleBRDF(rngState, sd, V_t, weight);// m_nested->sample(query_wp, sample);

    // did sampling fail?
    if (all(lessThanEqual(weight, vec3(0.0))) || any(isnan(weight)) || any(isinf(weight))) { return vec3(0.0); }

    wr = frameN * randomDirection;// wr = perturbed_wp.toWorld(query_wp.wo);

    float G1 = MFNM_G1(wp, wt, invFrameFaceN * wr);
    weight *= G1;// energy *= G1(wp, bRec.its.toLocal(wr));
  }

  return invFrameN * wr;
}

#endif

#endif // MFNM_GLSL
