#include "filesystem/blob.h"
#include "loaders/material.h"
#include "util.h"
#include "lib/vec/vec.h"

#pragma once

typedef struct {
  int material;
  int drawStart;
  int drawCount;
} ModelPrimitive;

typedef struct ModelNode {
  float transform[16];
  int parent;
  vec_uint_t children;
  vec_uint_t primitives;
} ModelNode;

typedef struct {
  ModelNode* nodes;
  ModelPrimitive* primitives;
  MaterialData* materials;
  float* vertices;
  void* indices;
  int nodeCount;
  int primitiveCount;
  int materialCount;
  int vertexCount;
  int vertexSize;
  int indexCount;
  int hasNormals;
  int hasUVs;
} ModelData;

ModelData* lovrModelDataCreate(Blob* blob);
void lovrModelDataDestroy(ModelData* modelData);
