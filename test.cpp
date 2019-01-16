#include "cgal.h"
#include <FronTier.h>
#include <string>
#include "folding.h"
#include "spring_solver.h"
#include <fenv.h>
#include <fstream>
#include <cstdlib>

//test module for folding algorithm
static void initTestModule(Front*,SURFACE*&);
static void initAirbag(Front*,std::ifstream&,SURFACE*&);
static void initFabric(Front*,std::ifstream&,SURFACE*&);
void initFabricCircle(Front* front, std::ifstream&, SURFACE* &surf);
void initFabricRectangle(Front* front, std::ifstream&, SURFACE* &surf);
bool findAndLocate(std::ifstream&, const char*);
bool getString(std::ifstream&, const char*);
void getMassPoints(INTERFACE*); 

int main(int argc, char** argv)
{
	feenableexcept(FE_INVALID | FE_OVERFLOW);
	static Front front;
	static F_BASIC_DATA f_basic;
        static LEVEL_FUNC_PACK level_func_pack;
        f_basic.dim = 3;
        FT_Init(argc,argv,&f_basic);
        f_basic.size_of_intfc_state = sizeof(STATE);

        /* Initialize basic computational data */
        char* in_name                 = f_basic.in_name;
        std::string out_name(f_basic.out_name);

	//initialize interface and velocity
        FT_ReadSpaceDomain(in_name,&f_basic);
        FT_StartUp(&front,&f_basic);
        FT_InitDebug(in_name);

        level_func_pack.pos_component = 2;
        FT_InitIntfc(&front,&level_func_pack);
		
	SURFACE* surf;
	initTestModule(&front,surf);
        getMassPoints(front.interf); 
	//Draw initial shape
	FT_Draw(&front);
	FT_AddTimeStepToCounter(&front);

	//initialize folding solver
	Folder* folder = new Folder3d(front.interf,surf, out_name);
	
	//adding folding plan from file
	std::string inname(in_name); 
	std::ifstream fin(in_name); 
        std::string mesg(inname);
        size_t pos = mesg.rfind('/');

        if (pos == std::string::npos)
            mesg.clear();
        else
            mesg = mesg.substr(0, pos + 1);
        if (!fin.is_open())
	{
	    std::cerr << "Can't open file " << inname << std::endl; 
	    clean_up(ERROR); 
	}

        if (!findAndLocate(fin, "Enter file path of folding plan:"))
	    clean_up(ERROR);
        
        std::string temp;

        fin >> temp;
        mesg += temp;
        std::cout << mesg << std::endl;
	folder->addDragsFromFile(mesg);
	folder->setInputFile(inname);
        fin.close(); 
	//set folding movie
	folder->setupMovie("fold_movie", OutName(&front), 0.05);

	//set numerical scheme for ode EXPLICIT or IMPLICIT
        //folder->setOdeScheme(SpringSolver::IMPLICIT);
        // change
        folder->setOdeScheme(SpringSolver::DIRECT);
        //folder->setOdeScheme(SpringSolver::EXPLICIT);

	//set spring parameters: k, lambda, m
	//default value
	folder->setSpringParameters(800, 0.04, 0.001);
        //get parameter value from file
        //if fail will use default value
	folder->setParaFromFile(in_name);
        

	//start to fold
	folder->doFolding();

	//FronTier draw results
	FT_Draw(&front);
	FT_AddTimeStepToCounter(&front);
	clean_up(0);
}

void initTestModule(Front* front, SURFACE* &surf) {
        std::ifstream fin(InName(front)); 
	char mesg[256];
	if (!findAndLocate(fin,"Enter problem type:"))
	    clean_up(ERROR);
        fin >> mesg; 
	std::cout << mesg << std::endl; 
	std::string prob_type(mesg);
	if (prob_type == "Airbag")
	    initAirbag(front,fin,surf);
	else if (prob_type == "Fabric")
	    initFabric(front,fin,surf);
	else
	    printf("Unknown type = %s\n",mesg);
        fin.close(); 
	//set Original Length
        TRI* t;
        surf_tri_loop(surf, t)
        {
            for (int i = 0; i < 3; ++i)
            {
                t->side_length0[i] = separation(Point_of_tri(t)[i],
                                Point_of_tri(t)[(i+1)%3], 3);
            }
        }
}

void initAirbag(Front* front, std::ifstream& fin, SURFACE* &surf) {
	double center[3], radius[3];
	if (!findAndLocate(fin,"Enter center of airbag:"))
	    clean_up(ERROR); 
	fin >> center[0] >> center[1] >> center[2]; 
	std::cout << center[0] << " " << center[1] << " " 
		<< center[2] << std::endl; 

	if (!findAndLocate(fin, "Enter radius of airbag:"))
	    clean_up(ERROR); 
	fin >> radius[0] >> radius[1] >> radius[2]; 
	std::cout << radius[0] << " " << radius[1] << " " 
		<< radius[2] << std::endl; 

	COMPONENT neg_comp = 2;
	COMPONENT pos_comp = 3;
	int w_type = ELASTIC_BOUNDARY;
	FT_MakeEllipticSurf(front,center,
                            radius,
                            neg_comp,pos_comp,
                            w_type,1,
                            &surf);
}

void initFabric(Front* front, std::ifstream& fin, SURFACE* &surf) {
	std::string shape("Circle");

	if (!findAndLocate(fin,"Enter fabric shape:"))
 	    std::cout << "use defalut\n"; 
        fin >> shape; 
	std::cout << shape << std::endl; 
	if ("Circle" == shape)
	    initFabricCircle(front, fin, surf);
	else if ("Rectangle" == shape)
	    initFabricRectangle(front, fin, surf);
	else if ("CGAL" == shape)
        {
	    cgalSurf* crs;
            std::string cgalShape;
            if (!findAndLocate(fin,"Enter the shape generated by CGAL:")) 
		clean_up(ERROR); 
            fin >> cgalShape; 
	    std::cout << cgalShape << std::endl;
            switch (cgalShape[0])
            {
		case 'R' :
		case 'r' : crs = new cgalRectangleSurf(front->interf, &surf); 
				 break; 
                case 'C' :
		case 'c' : crs = new cgalCircleSurf(front->interf, &surf);
			   break; 
		case 'P' :
		case 'p' : crs = new cgalParaSurf(front->interf, &surf); 
			   break;  
		default : std::cout << "Unknown fabric shape\n"; 
			  clean_up(ERROR);
	    }
	   
            crs->cgalTriMesh(fin); 
        }
	else
	{
	    std::cout << "Unknown fabric shape " << shape << std::endl;
	    clean_up(ERROR); 
	}
}

void initFabricCircle(Front* front, std::ifstream& fin, SURFACE* &surf)
{
	static PLANE_PARAMS plane_params;
        static LEVEL_FUNC_PACK level_func_pack;
        if (!findAndLocate(fin,"Enter the height of the plane:"))
	    clean_up(ERROR);
	fin >> plane_params.P[2];
	std::cout << plane_params.P[2] << std::endl; 
        plane_params.N[0] = plane_params.N[1] = 0.0;
        plane_params.N[2] = 1.0;

        static CIRCLE_PARAMS circle_constr_params;

        level_func_pack.wave_type = ELASTIC_BOUNDARY;
        level_func_pack.set_3d_bdry = YES;
        level_func_pack.neg_component = 3;
        level_func_pack.pos_component = 3;
        level_func_pack.func_params = (POINTER)&plane_params;
        level_func_pack.func = plane_func;
        level_func_pack.is_mono_hs = YES;
        level_func_pack.constr_params = (POINTER)&circle_constr_params;
        level_func_pack.constr_func = circle_constr_func;
        level_func_pack.num_mono_hs = 1;
        if (!findAndLocate(fin,"Enter circle center:"))
	    clean_up(ERROR); 
	fin >> circle_constr_params.cen[0] >> circle_constr_params.cen[1]; 
	std::cout << circle_constr_params.cen[0] << " " 
		<< circle_constr_params.cen[1] << std::endl; 
        if (!findAndLocate(fin,"Enter circle radius:"))
	    clean_up(ERROR); 
	fin >> circle_constr_params.R; 
	std::cout << circle_constr_params.R << std::endl; 
        FT_InitIntfc(front,&level_func_pack);
        SURFACE** s;
        intfc_surface_loop(front->interf,s) {
            if (wave_type(*s) == level_func_pack.wave_type) {
                surf = *s;
                break;
            }
        }
}

void initFabricRectangle(Front* front, std::ifstream& fin, SURFACE* &surf)
{
	static PLANE_PARAMS plane_params;
        static LEVEL_FUNC_PACK level_func_pack;
        if (!findAndLocate(fin,"Enter the height of the plane:"))
	    clean_up(ERROR);
	fin >> plane_params.P[2]; 
	std::cout << plane_params.P[2] << std::endl; 
        plane_params.N[0] = plane_params.N[1] = 0.0;
        plane_params.N[2] = 1.0;
        

        static RECT_CONSTR_PARAMS rect_constr_params;
	rect_constr_params.dim = 3;

        level_func_pack.wave_type = ELASTIC_BOUNDARY;
        level_func_pack.set_3d_bdry = YES;
        level_func_pack.neg_component = 3;
        level_func_pack.pos_component = 3;
        level_func_pack.func_params = (POINTER)&plane_params;
        level_func_pack.func = plane_func;
        level_func_pack.is_mono_hs = YES;
        level_func_pack.constr_params = (POINTER)&rect_constr_params;
        level_func_pack.constr_func = rect_constr_func;
        level_func_pack.num_mono_hs = 1;
        if (!findAndLocate(fin,"Enter lower boundary of plane:"))
	    clean_up(ERROR); 
	fin >> rect_constr_params.L[0] >> rect_constr_params.L[1] 
			>> rect_constr_params.L[2]; 
	std::cout << rect_constr_params.L[0] << " " << rect_constr_params.L[1] 
		<< " " << rect_constr_params.L[2] << std::endl; 
        if (!findAndLocate(fin,"Enter upper boundary of plane:"))
	    clean_up(ERROR); 
	fin >> rect_constr_params.U[0] >> rect_constr_params.U[1] 
		>> rect_constr_params.U[2];
	std::cout << rect_constr_params.U[0] << " " << rect_constr_params.U[1]
		<< " " << rect_constr_params.U[2] << std::endl;
        FT_InitIntfc(front,&level_func_pack);
        SURFACE** s;
        intfc_surface_loop(front->interf,s) {
            if (wave_type(*s) == level_func_pack.wave_type) {
                surf = *s;
                break;
            }
        }
}

bool findAndLocate(std::ifstream& fin, const char* mesg)
{
    std::cout << mesg << " "; 
    if (!getString(fin, mesg))
    {
        fin.seekg(0);
        if (!getString(fin, mesg))
        {
            std::cout << "Can't find string " << mesg << " in file!\n";
            return false;
        }
    }
    return true;
}

bool getString(std::ifstream& fin, const char* mesg)
{
    std::streampos position;
    std::string line;

    position = fin.tellg();
    while (getline(fin, line))
    {
        if (line.find(mesg) != std::string::npos)
        {
            char ch;
            fin.seekg(-1, std::ios_base::cur);
            while (fin.get(ch) && ch != ':')
                fin.seekg(-2, std::ios_base::cur);
            fin.seekg(1, std::ios_base::cur);
            return true;
        }
    }
    fin.clear();
    fin.seekg(position);
    return false;
}

void getMassPoints(INTERFACE* intfc) {
    SURFACE** s; 
    std::ofstream fout("massPoint.txt"); 

    if (!fout.is_open()) {
        std::cout << "Open file error!\n"; 
        exit(EXIT_FAILURE); 
    }

    intfc_surface_loop(intfc, s) {
        if (wave_type(*s) != ELASTIC_BOUNDARY)
            continue; 

        TRI* tri; 

        surf_tri_loop(*s, tri) 
            for (int i = 0; i < 3; i++)
                 sorted(Point_of_tri(tri)[i]) = NO; 
        surf_tri_loop(*s, tri) {
            for (int i = 0; i < 3; i++) {
                 POINT* p = Point_of_tri(tri)[i]; 

                 if (sorted(p)) continue; 
                 sorted(p) = YES; 
                 for (int j = 0; j < 3; j++)  
                      fout << Coords(p)[j] << ' '; 
                 fout << std::endl; 
            }
        }
    } 
    fout.close(); 
}
