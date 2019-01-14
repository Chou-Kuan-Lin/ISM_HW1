#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <numeric>
#include <iostream>
#include <Windows.h>
#include <gl/GL.h>
#include <gl/glut.h>

#include "glm.h"
#include "mtxlib.h"
#include "trackball.h"

using namespace std;

_GLMmodel *mesh;
int WindWidth, WindHeight;

int last_x , last_y;
int selectedFeature = -1;
vector<int> featureList;

vector<float> WiVi;	//權重
vector<float> Radial;	//計算選取點與其餘點之距離=P-Pi
vector<float> Distance;	//抓取之Feature有值，其餘為0
//vector<float> Deformation;	//紀錄所有點之形變量=WiVi*Radial
float sigma = 0.5;
vector<float> OriginalInfo;	//vertice原始位置儲存

void showMatrix(vector<float> matrix)
{
	//printf("==========\n");
	int n = pow(matrix.size(), 0.5);
	for (int a = 0; a < n; a++)
	{
		for (int b = 0; b < n; b++)
		{
			printf("%f ", matrix[a*n + b]);
		}
		printf("\n");
	}
	printf("==========\n");
}

//Radial basis function
float RadialF(int Pi, int P)
{
	float Pi_x, Pi_y, Pi_z, P_x, P_y, P_z;
	float r;
	//Pi_x = mesh->vertices[Pi * 3 + 0];
	//Pi_y = mesh->vertices[Pi * 3 + 1];
	//Pi_z = mesh->vertices[Pi * 3 + 2];
	//P_x = mesh->vertices[P * 3 + 0];
	//P_y = mesh->vertices[P * 3 + 1];
	//P_z = mesh->vertices[P * 3 + 2];
	Pi_x = OriginalInfo[Pi * 3 + 0];
	Pi_y = OriginalInfo[Pi * 3 + 1];
	Pi_z = OriginalInfo[Pi * 3 + 2];
	P_x = OriginalInfo[P * 3 + 0];
	P_y = OriginalInfo[P * 3 + 1];
	P_z = OriginalInfo[P * 3 + 2];

	r = pow(pow(Pi_x - P_x, 2) + pow(Pi_y - P_y, 2) + pow(Pi_z - P_z, 2), 0.5);
	
	return exp(((-1)*(pow(fabs(r), 2)) / (2 * pow(sigma, 2))));
}

//高斯約當法(反矩陣)
vector <float> MatrixOperation(vector <float> matrix)
{
	//矩陣大小(Feature數量)
	int n = pow(matrix.size(), 0.5);

	//矩陣宣告、定值
	vector <float> unit;

	for (int a = 0; a < n; a++)
		for (int b = 0;b < n;b++)
			if (a == b)
				unit.push_back(1);
			else
				unit.push_back(0);

	//高斯列運算
	//上三角矩陣
	float times = 0;
	float minus = 0;
	for (int a = 0; a < n; a++)	//列數
	{
		//使i*i之前項為0
		for (int b = 0;b < a;b++)	//為1的行數
		{
			minus = matrix[a*n + b] / matrix[b*n + b];
			for (int c = 0;c < n;c++)	//行數
			{
				matrix[a*n + c] -= minus * matrix[b*n + c];
				unit[a*n + c] -= minus * unit[b*n + c];
			}
		}

		//使i*i為1
		times = matrix[a*n + a];
		for (int d = 0; d < n; d++)	//行數
		{
			matrix[a*n + d] /= times;
			unit[a*n + d] /= times;
		}
	}

	//下三角矩陣
	for (int a = n - 1;a >= 0; a--)	//為1的列數
	{
		//使i*i之後項為0
		for (int c = a - 1;c >= 0;c--)	//列數
		{
			minus = matrix[c*n + a] / matrix[a*n + a];
			for (int d = 0;d < n;d++)	//行數
			{
				matrix[c*n + d] -= minus * matrix[a*n + d];
				unit[c*n + d] -= minus * unit[a*n + d];
			}
		}
	}

	bool check = false;	//確認是否為單元矩陣
	for (int a = 0;a < n;a++)	//行數
		for (int b = 0;b < n; b++)	//列數
			if (matrix[a*n + b] != matrix[a*n + b] || matrix[a*n + b] != 0)	//#nan(ind) or not 0
				if (a == b && matrix[a*n + b] != 1)	//i*i=1
					check = true;

	if (check == true)
		for (int i = 0;i < unit.size();i++)
			unit[i] = 0.000001;
	return unit;
}

void Reshape(int width, int height)
{
  int base = min(width , height);

  tbReshape(width, height);
  glViewport(0 , 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45.0,(GLdouble)width / (GLdouble)height , 1.0, 128.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -3.5);

  WindWidth = width;
  WindHeight = height;
}

void Display(void)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  tbMatrix();
  
  // render solid model
  glEnable(GL_LIGHTING);
  glColor3f(1.0 , 1.0 , 1.0f);
  glPolygonMode(GL_FRONT_AND_BACK , GL_FILL);
  glmDraw(mesh , GLM_SMOOTH);

  // render wire model
  glPolygonOffset(1.0 , 1.0);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glLineWidth(1.0f);
  glColor3f(0.6 , 0.0 , 0.8);
  glPolygonMode(GL_FRONT_AND_BACK , GL_LINE);
  glmDraw(mesh , GLM_SMOOTH);

  // render features
  glPointSize(10.0);
  glColor3f(1.0 , 0.0 , 0.0);
  glDisable(GL_LIGHTING);
  glBegin(GL_POINTS);
	for (int i = 0 ; i < featureList.size() ; i++)
	{
		int idx = featureList[i];
		glVertex3fv((float *)&mesh->vertices[3 * idx]);
	}
  glEnd();
  
  glPopMatrix();

  glFlush();  
  glutSwapBuffers();
}

vector3 Unprojection(vector2 _2Dpos)
{
	float Depth;
	int viewport[4];
	double ModelViewMatrix[16];				//Model_view matrix
	double ProjectionMatrix[16];			//Projection matrix

	glPushMatrix();
	tbMatrix();

	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetDoublev(GL_MODELVIEW_MATRIX, ModelViewMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, ProjectionMatrix);

	glPopMatrix();

	glReadPixels((int)_2Dpos.x , viewport[3] - (int)_2Dpos.y , 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &Depth);

	double X = _2Dpos.x;
	double Y = _2Dpos.y;
	double wpos[3] = {0.0 , 0.0 , 0.0};

	gluUnProject(X , ((double)viewport[3] - Y) , (double)Depth , ModelViewMatrix , ProjectionMatrix , viewport, &wpos[0] , &wpos[1] , &wpos[2]);

	return vector3(wpos[0] , wpos[1] , wpos[2]);
}

void mouse(int button, int state, int x, int y)
{
  tbMouse(button, state, x, y);
  
  // add feature
  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
  {
	  int minIdx = 0;
	  float minDis = 9999999.0f;
	  
	  vector3 pos = Unprojection(vector2((float)x , (float)y));	//滑鼠位置
	  
	  for (int i = 0 ; i < mesh->numvertices ; i++)
	  {
		  vector3 pt(mesh->vertices[3 * i + 0] , mesh->vertices[3 * i + 1] , mesh->vertices[3 * i + 2]);
		  float dis = (pos - pt).length();

		  if (minDis > dis)
		  {
			  minDis = dis;
			  minIdx = i;
		  }
	  }
	  
	  featureList.push_back(minIdx);	//被列入Feature的頂點編號
  }

  // manipulate feature
  if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
  {
	  int minIdx = 0;
	  float minDis = 9999999.0f;

	  vector3 pos = Unprojection(vector2((float)x , (float)y));	//滑鼠位置

	  for (int i = 0 ; i < featureList.size() ; i++)
	  {
		  int idx = featureList[i];	//Feature編號

		  vector3 pt(mesh->vertices[3 * idx + 0] , mesh->vertices[3 * idx + 1] , mesh->vertices[3 * idx + 2]);
		  float dis = (pos - pt).length();	//每一個Feature對滑鼠座標的距離
		  		  
		  if (minDis > dis)
		  {
			  minDis = dis;
			  minIdx = featureList[i];
		  }
	  }

	  selectedFeature = minIdx;

	  /*
	  printf("mesh->numvertices=%i\n", mesh->numvertices);	//總頂點數=2677
	  printf("featureList.size()=%i\n", featureList.size());	//列入List的Feature數
	  printf("mesh->vertices=%i\n", mesh->vertices);	//(x,y,z)
	  */
  }

  if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP)
  {
	  selectedFeature = -1;
  }

  last_x = x;
  last_y = y;
}

void motion(int x, int y)	//滑鼠位置
{
  tbMotion(x, y);
  if (selectedFeature != -1)
  {
	  matrix44 m;
	  vector4 vec = vector4((float)(x - last_x) / 100.0f, (float)(y - last_y) / 100.0f, 0.0, 1.0);

	  gettbMatrix((float *)&m);
	  vec = m * vec;	//移動方向之向量

	  //選取Feature編號附近三點編號矩陣的點座標，加上滑鼠移動之向量
	  //mesh->vertices[3 * selectedFeature + 0] += vec.x;	//左右
	  //mesh->vertices[3 * selectedFeature + 1] -= vec.y;	//上下
	  //mesh->vertices[3 * selectedFeature + 2] += vec.z;

	  printf("%f\t%f\t%f\n", vec.x, vec.y, vec.z);

	  //WiVi[i] = Distance[i] * MatrixOperation(Radial);
	  int n = featureList.size();	//階數
	  float product_x = 0;	//內積和
	  float product_y = 0;
	  float product_z = 0;

	  //d matrix
	  Distance.clear();
	  for (int i = 0;i < n;i++)
		  for (int j = 0;j < 3;j++)
			  Distance.push_back(0);
	  //移動之Feature有值
	  for (int i = 0;i < n;i++)
		  if (featureList[i] == selectedFeature)
		  {
			  Distance[i * 3 + 0] = vec.x;
			  Distance[i * 3 + 1] = vec.y;
			  Distance[i * 3 + 2] = vec.z;
			  i = n;
		  }

	  //M matrix(Feature)
	  Radial.clear();
	  for (int i = 0;i < n;i++)
		  for (int j = 0;j < n;j++)
			  Radial.push_back(RadialF(featureList[i], featureList[j]));
	  Radial = MatrixOperation(Radial);	//M^(-1)

	  //WiVi=M^(-1)*d
	  WiVi.clear();
	  for (int a = 0;a < n;a++)
	  {
		  product_x = 0;
		  product_y = 0;
		  product_z = 0;
		  for (int b = 0;b < n;b++)
		  {
			  product_x += Radial[a * n + b] * Distance[a * 3 + 0];
			  product_y += Radial[a * n + b] * Distance[a * 3 + 1];
			  product_z += Radial[a * n + b] * Distance[a * 3 + 2];
		  }
		  WiVi.push_back(product_x);
		  WiVi.push_back(product_y);
		  WiVi.push_back(product_z);
	  }

	  //each vertice deformation
	  //d' = sigma(WiVi*Fi)
	  for (int a = 0;a < mesh->numvertices;a++)
	  {
		  product_x = 0;
		  product_y = 0;
		  product_z = 0;
		  for (int i = 0;i < n;i++)
		  {
			  product_x += WiVi[i * 3 + 0] * RadialF(featureList[i], a);
			  product_y += WiVi[i * 3 + 1] * RadialF(featureList[i], a);
			  product_z += WiVi[i * 3 + 2] * RadialF(featureList[i], a);
		  }
		  mesh->vertices[a * 3 + 0] += product_x;
		  mesh->vertices[a * 3 + 1] -= product_y;
		  mesh->vertices[a * 3 + 2] += product_z;
	  }
  }
  last_x = x;
  last_y = y;
}

void timf(int value)
{
  glutPostRedisplay();
  glutTimerFunc(1, timf, 0);
}

int main(int argc, char *argv[])
{
  WindWidth = 400;
  WindHeight = 400;
	
  GLfloat light_ambient[] = {0.0, 0.0, 0.0, 1.0};
  GLfloat light_diffuse[] = {0.8, 0.8, 0.8, 1.0};
  GLfloat light_specular[] = {1.0, 1.0, 1.0, 1.0};
  GLfloat light_position[] = {0.0, 0.0, 1.0, 0.0};

  glutInit(&argc, argv);
  glutInitWindowSize(WindWidth, WindHeight);
  glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
  glutCreateWindow("Trackball Example");

  glutReshapeFunc(Reshape);
  glutDisplayFunc(Display);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glClearColor(0, 0, 0, 0);

  glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
  glLightfv(GL_LIGHT0, GL_POSITION, light_position);

  glEnable(GL_LIGHT0);
  glDepthFunc(GL_LESS);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glEnable(GL_COLOR_MATERIAL);
  tbInit(GLUT_MIDDLE_BUTTON);
  tbAnimate(GL_TRUE);

  glutTimerFunc(40, timf, 0); // Set up timer for 40ms, about 25 fps

  // load 3D model
  mesh = glmReadOBJ("../data/head.obj");
  
  glmUnitize(mesh);
  glmFacetNormals(mesh);
  glmVertexNormals(mesh , 90.0);

  //儲存原始MeshInfo.
  for (int i = 0;i < mesh->numvertices * 3 + 2;i++)
	  OriginalInfo.push_back(mesh->vertices[i]);

  glutMainLoop();	//循環點

  return 0;

}

