#include "treeseg.h"

#include <pcl/io/pcd_io.h>
#include <pcl/common/common.h>

int main(int argc, char **argv)
{
	std::vector<std::string> args(argv+1,argv+argc);
	//
	pcl::PCDReader reader;
	pcl::PCDWriter writer;
	std::stringstream ss;
	//
	std::cout << "Reading slice: " << std::flush;
	std::vector<std::string> id = getFileID(args[4]);
	pcl::PointCloud<PointTreeseg>::Ptr slice(new pcl::PointCloud<PointTreeseg>);
	reader.read(args[4],*slice);
	std::cout << "complete" << std::endl;
	std::cout << "read " << slice->size() << " points" << std::endl;
	//
	std::cout << "Cluster extraction: " << std::flush;
	std::vector<pcl::PointCloud<PointTreeseg>::Ptr> clusters;
	int nnearest = 18;
	int nmin = 100;
	std::vector<float> nndata = dNN(slice, nnearest);
	std::cout << "Clusters dNN: " << nndata.size() << std::endl;
	std::cout << "mean: " << nndata[0] << " std: " << nndata[1] << std::endl;
	euclideanClustering(slice,nndata[0],nmin,clusters);
	std::cout << "euclideanClustering done. clusters.size(): " << clusters.size() << std::endl;
	ss.str("");
	ss << id[0] << ".intermediate.slice.clusters.pcd";
	writeClouds(clusters,ss.str(),false);
	std::cout << ss.str() << " | " << clusters.size() << std::endl;
	//
	std::cout << "Region-based segmentation: " << std::flush;
	std::vector<pcl::PointCloud<PointTreeseg>::Ptr> regions;
	pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
	float smoothness = std::stof(args[0]);
	for(int i=0;i<clusters.size();i++)
	{
		std::vector<pcl::PointCloud<PointTreeseg>::Ptr> tmpregions;
		estimateNormals(clusters[i],50,normals);
		regionSegmentation(clusters[i],normals,250,100,std::numeric_limits<int>::max(),smoothness,2,tmpregions);
		for(int j=0;j<tmpregions.size();j++) regions.push_back(tmpregions[j]);
		normals->clear();
	}
	ss.str("");
	ss << id[0] << ".intermediate.slice.clusters.regions.pcd";
	writeClouds(regions,ss.str(),false);
	std::cout << ss.str() << " | " << regions.size() << std::endl;
	//
	std::cout << "RANSAC cylinder fits: " << std::flush;
	std::vector<std::pair<float,pcl::PointCloud<PointTreeseg>::Ptr>> cylinders;
	nnearest = 60;
	float dmin = std::stof(args[1]);
	float dmax = std::stof(args[2]);
	std::cout << "opening file: " << args[3] << std::endl;
	std::ifstream coordfile;
	coordfile.open(args[3]);
	float coords[4];
	int n = 0;
	if(coordfile.is_open())
	{
		while(!coordfile.eof())
		{
			coordfile >> coords[n];
			n++;
			std::cout << "reading coord " << n-1 << " value: " << coords[n-1] << std::endl;
		}
	}
	coordfile.close();
	float xmin = coords[0];
	float xmax = coords[1];
	float ymin = coords[2];
	float ymax = coords[3];
	std::cout << "read " << args[3] << ": x min, max" << xmin << ", " << xmax << "  y min, max" << ymin << ", " << ymax << std::endl;
	float lmin = 2.5; //assumes 3m slice
	float stepcovmax = 0.1;
	float radratiomin = 0.9;
	for(int i=0;i<regions.size();i++)
	{
	    std::cout << "handling region " << i << " of " << regions.size() << std::endl;
		cylinder cyl;
		fitCylinder(regions[i],nnearest,true,true,cyl);
		//std::cout << cyl.ismodel << " " << cyl.rad << " " << cyl.len << " " << cyl.stepcov << " " << cyl.radratio << " " << cyl.x << " " << cyl.y << std::endl;
		if(cyl.ismodel == true)
		{		
			if(cyl.rad*2 >= dmin && cyl.rad*2 <= dmax && cyl.len >= lmin)
			{
				if(cyl.stepcov <= stepcovmax && cyl.radratio > radratiomin)
				{
					if(cyl.x >= xmin && cyl.x <= xmax)
					{
						if(cyl.y >= ymin && cyl.y <= ymax)
						{
							cylinders.push_back(std::make_pair(cyl.rad,cyl.inliers));
						}
					}
				}
			}
		}
	}
	std::sort(cylinders.rbegin(),cylinders.rend());
	std::vector<pcl::PointCloud<PointTreeseg>::Ptr> cyls;
	for(int i=0;i<cylinders.size();i++) cyls.push_back(cylinders[i].second);
	std::cout << "start writing cylinders" << std::endl;
	ss.str("");
	ss << id[0] << ".intermediate.slice.clusters.regions.cylinders.pcd";
	writeClouds(cyls,ss.str(),false);
	std::cout << ss.str() << " | " << cyls.size() << std::endl;
	//
	std::cout << "Principal component trimming: " << std::flush;
	float anglemax = 35;
	std::vector<int> idx;
	for(int j=0;j<cyls.size();j++)
	{
		Eigen::Vector4f centroid;
		Eigen::Matrix3f covariancematrix;
		Eigen::Matrix3f eigenvectors;
		Eigen::Vector3f eigenvalues;
		computePCA(cyls[j],centroid,covariancematrix,eigenvectors,eigenvalues);
		Eigen::Vector4f gvector(eigenvectors(0,2),eigenvectors(1,2),0,0);
		Eigen::Vector4f cvector(eigenvectors(0,2),eigenvectors(1,2),eigenvectors(2,2),0);
		float angle = pcl::getAngle3D(gvector,cvector) * (180/M_PI);
		if(angle >= (90 - anglemax) && angle <= (90 + anglemax)) idx.push_back(j);
	}
	std::vector<pcl::PointCloud<PointTreeseg>::Ptr> pca;
        for(int k=0;k<idx.size();k++) pca.push_back(cyls[idx[k]]);	
	ss.str("");
	ss << id[0] << ".intermediate.slice.clusters.regions.cylinders.principal.pcd";
	writeClouds(pca,ss.str(),false);
	std::cout << ss.str() << " | " << pca.size() << std::endl;
	//
	std::cout << "Concatenating stems: " << std::flush;
	float expansionfactor = 0;
	std::vector<pcl::PointCloud<PointTreeseg>::Ptr> stems;
	stems = pca;
	catIntersectingClouds(stems);
	ss.str("");
	ss << id[0] << ".intermediate.slice.clusters.regions.cylinders.principal.cat.pcd";
	writeClouds(stems,ss.str(),false);
	for(int m=0;m<stems.size();m++)
	{
		ss.str("");
		ss << id[0] << ".cluster." << m << ".pcd";
		writer.write(ss.str(),*stems[m],true);
	}
	std::cout << stems.size() << std::endl;
	//
	return 0;
}
